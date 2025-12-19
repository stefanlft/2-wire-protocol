#include <cmocka.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../includes/data_link.h"
#include "../includes/packets.h"
#include "../includes/statuses.h"

// For testing static functions
#include "../src/data_link.c"

static void test_twp_set_seq_num(void **state) {
    (void)state; /* unused */
    struct data_link_t data_link = {.seq_num = 42};
    struct packet_t packet = {0};

    uint8_t status = twp_set_seq_num(&data_link, &packet);

    assert_int_equal(status, STATUS_OK);
    assert_int_equal(packet.seq_num, 42);
    assert_int_equal(data_link.seq_num, 43);
}

static void test_twp_set_checksum(void **state) {
    (void)state; /* unused */
    struct data_link_t data_link = {0};
    uint8_t data[] = "test_data";
    struct packet_t packet = {
        .data = data,
        .length = sizeof(data) - 1 // exclude null terminator
    };

    uint8_t status = twp_set_checksum(&data_link, &packet);

    assert_int_equal(status, STATUS_OK);
    uint32_t expected_checksum =
        crc32(0L, (const Bytef *)data, sizeof(data) - 1);
    assert_int_equal(packet.checksum, expected_checksum);
}

static void test_check_addr(void **state) {
    (void)state; /* unused */
    struct data_link_t data_link = {.node_address = 0x10};
    struct packet_t pkt;

    // Test broadcast address
    pkt.receiver_address = BROADCAST_ADDRESS;
    assert_int_equal(check_addr(&data_link, &pkt), 1);

    // Test loopback address
    pkt.receiver_address = LOOPBACK_ADDRESS;
    assert_int_equal(check_addr(&data_link, &pkt), 1);

    // Test direct node address
    pkt.receiver_address = 0x10;
    assert_int_equal(check_addr(&data_link, &pkt), 1);

    // Test wrong address
    pkt.receiver_address = 0x20;
    assert_int_equal(check_addr(&data_link, &pkt), 0);
}

static void test_twp_pack_unpack(void **state) {
    (void)state; /* unused */
    struct data_link_t data_link = {0};
    uint8_t message[] = "Hello, world!";
    struct packet_t packet_out = {.sender_address = 0x01,
                                  .receiver_address = 0x02,
                                  .packet_type = 0xAA,
                                  .seq_num = 123,
                                  .length = sizeof(message),
                                  .data = message,
                                  .checksum = 0xDEADBEEF};

    uint8_t *buffer = NULL;
    size_t size = 0;

    uint8_t pack_status = twp_pack(&data_link, &packet_out, &buffer, &size);
    assert_int_equal(pack_status, STATUS_OK);
    assert_non_null(buffer);

    struct packet_t packet_in = {0};
    uint8_t unpack_status = twp_unpack(&data_link, buffer, size, &packet_in);
    assert_int_equal(unpack_status, STATUS_OK);
    assert_non_null(packet_in.data);

    assert_int_equal(packet_in.sender_address, packet_out.sender_address);
    assert_int_equal(packet_in.receiver_address, packet_out.receiver_address);
    assert_int_equal(packet_in.packet_type, packet_out.packet_type);
    assert_int_equal(packet_in.seq_num, packet_out.seq_num);
    assert_int_equal(packet_in.length, packet_out.length);
    assert_int_equal(packet_in.checksum, packet_out.checksum);
    assert_memory_equal(packet_in.data, packet_out.data, packet_out.length);

    free(buffer);
    free(packet_in.data);
}

static void test_twp_send_recv_raw(void **state) {
    (void)state; /* unused */

    // Use memory streams for testing I/O
    char *stream_buf = NULL;
    size_t stream_size = 0;
    FILE *output_stream = open_memstream(&stream_buf, &stream_size);
    assert_non_null(output_stream);

    struct data_link_t data_link_send = {
        .node_address = 0x01, .output_stream = output_stream, .seq_num = 0};

    uint8_t message[] = "This is a test message";
    struct packet_t packet_out = {.receiver_address = 0x02,
                                  .packet_type = 0xBB,
                                  .data = message,
                                  .length = sizeof(message)};

    uint8_t send_status = twp_send(&data_link_send, &packet_out);
    assert_int_equal(send_status, STATUS_OK);
    fclose(
        output_stream); // This flushes the stream and makes buffer/size valid

    // Now, prepare to receive
    FILE *input_stream = fmemopen(stream_buf, stream_size, "r");
    assert_non_null(input_stream);

    struct data_link_t data_link_recv = {.input_stream = input_stream};
    struct packet_t packet_in = {0};

    uint8_t recv_status = twp_recv_raw(&data_link_recv, &packet_in);
    assert_int_equal(recv_status, STATUS_OK);

    // Validate received packet
    assert_int_equal(packet_in.sender_address, data_link_send.node_address);
    assert_int_equal(packet_in.receiver_address, packet_out.receiver_address);
    assert_int_equal(packet_in.packet_type, packet_out.packet_type);
    assert_int_equal(packet_in.seq_num, 0); // First sequence number
    assert_int_equal(packet_in.length, packet_out.length);
    assert_memory_equal(packet_in.data, packet_out.data, packet_out.length);

    // Checksum was calculated by twp_send and verified by twp_recv_raw
    uint32_t expected_checksum =
        crc32(0L, (const Bytef *)packet_in.data, packet_in.length);
    assert_int_equal(packet_in.checksum, expected_checksum);

    free(packet_in.data);
    fclose(input_stream);
    free(stream_buf);
}

static void test_twp_recv_raw_checksum_mismatch(void **state) {
    (void)state; /* unused */

    // Create a dummy packet buffer
    uint8_t message[] = "bad data";
    struct packet_t packet_out = {
        .sender_address = 0x01,
        .receiver_address = 0x02,
        .packet_type = 0xCC,
        .seq_num = 10,
        .length = sizeof(message),
        .data = message,
        .checksum = 0 // Incorrect checksum
    };

    uint8_t *buffer = NULL;
    size_t size = 0;
    twp_pack(NULL, &packet_out, &buffer, &size);

    FILE *input_stream = fmemopen(buffer, size, "r");
    assert_non_null(input_stream);

    struct data_link_t data_link_recv = {.input_stream = input_stream};
    struct packet_t packet_in = {0};

    uint8_t recv_status = twp_recv_raw(&data_link_recv, &packet_in);
    assert_int_equal(recv_status, STATUS_ERR_CHECKSUM_MISMATCH);

    // packet_in.data should not have been allocated or should have been freed
    // The implementation of twp_recv_raw frees on checksum failure.

    fclose(input_stream);
    free(buffer);
}

static void test_twp_validate(void **state) {
    (void)state; /* unused */
    struct data_link_t data_link = {.seq_num = 99};
    uint8_t data[] = "validate me";
    struct packet_t packet = {
        .data = data,
        .length = sizeof(data),
        .seq_num = 100 // Expected next sequence number
    };
    packet.checksum = crc32(0L, (const Bytef *)packet.data, packet.length);

    // Test valid packet
    uint8_t status = twp_validate(&data_link, &packet);
    assert_int_equal(status, STATUS_OK);
    assert_int_equal(data_link.seq_num, 100);

    // Test checksum mismatch
    packet.checksum = 0;
    status = twp_validate(&data_link, &packet);
    assert_int_equal(status, STATUS_ERR_CHECKSUM_MISMATCH);
    assert_int_equal(data_link.seq_num, 100); // Should not increment on failure

    // Test sequence number mismatch
    packet.checksum = crc32(0L, (const Bytef *)packet.data, packet.length);
    packet.seq_num = 102;
    status = twp_validate(&data_link, &packet);
    assert_int_equal(status, STATUS_ERR_SEQ_NUM_MISMATCH);
    assert_int_equal(data_link.seq_num, 100); // Should not increment on failure
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_twp_set_seq_num),
        cmocka_unit_test(test_twp_set_checksum),
        cmocka_unit_test(test_check_addr),
        cmocka_unit_test(test_twp_pack_unpack),
        cmocka_unit_test(test_twp_send_recv_raw),
        cmocka_unit_test(test_twp_recv_raw_checksum_mismatch),
        cmocka_unit_test(test_twp_validate),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}