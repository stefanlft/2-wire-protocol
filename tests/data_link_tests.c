#include "../includes/data_link.h"
#include "../includes/packets.h"
#include "../includes/statuses.h"
#include <cmocka.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* A test case for data_link_init */
static void test_data_link_init(void **state) {
    (void)state; /* Unused */
    struct data_link_t dl;
    FILE *in = tmpfile();
    FILE *out = tmpfile();
    assert_non_null(in);
    assert_non_null(out);

    data_link_init(&dl, in, out);

    assert_ptr_equal(dl.input_stream, in);
    assert_ptr_equal(dl.output_stream, out);
    assert_int_equal(dl.seq_num, 0);

    fclose(in);
    fclose(out);
}

/* A test case for twp_set_seq_num */
static void test_twp_set_seq_num(void **state) {
    (void)state; /* Unused */
    struct data_link_t dl;
    data_link_init(&dl, NULL, NULL);
    assert_int_equal(dl.seq_num, 0);

    struct packet_t pkt;

    uint8_t status = twp_set_seq_num(&dl, &pkt);
    assert_int_equal(status, STATUS_OK);
    assert_int_equal(pkt.seq_num, 0);
    assert_int_equal(dl.seq_num, 1);

    status = twp_set_seq_num(&dl, &pkt);
    assert_int_equal(status, STATUS_OK);
    assert_int_equal(pkt.seq_num, 1);
    assert_int_equal(dl.seq_num, 2);
}

/* A test case for twp_set_checksum */
static void test_twp_set_checksum(void **state) {
    (void)state;           /* Unused */
    struct data_link_t dl; // Not used by the function but part of the API
    struct packet_t pkt;
    uint8_t data[] = "hello";
    pkt.data = data;
    pkt.length = sizeof(data);

    uint8_t status = twp_set_checksum(&dl, &pkt);

    assert_int_equal(status, STATUS_OK);
    // crc32 for "hello" with null terminator is 0x9A84642D
    assert_int_equal(pkt.checksum, crc32(0, pkt.data, pkt.length));
}

/* A test case for packing and unpacking a packet */
static void test_twp_pack_unpack(void **state) {
    (void)state;           /* Unused */
    struct data_link_t dl; // Not used, but for API consistency
    struct packet_t packet_out;
    uint8_t message[] = "this is a test message";

    packet_out.sender_address = 0x01;
    packet_out.receiver_address = 0x02;
    packet_out.packet_type = 0xAA;
    packet_out.seq_num = 123;
    packet_out.length = sizeof(message);
    packet_out.data = message;
    packet_out.checksum = 0xDEADBEEF;

    uint8_t *buffer = NULL;
    size_t size = 0;

    uint8_t pack_status = twp_pack(&dl, &packet_out, &buffer, &size);
    assert_int_equal(pack_status, STATUS_OK);
    assert_non_null(buffer);
    assert_true(size > 0);

    struct packet_t packet_in;
    // We need to free packet_in.data later
    packet_in.data = NULL;

    uint8_t unpack_status = twp_unpack(&dl, buffer, size, &packet_in);
    assert_int_equal(unpack_status, STATUS_OK);
    assert_non_null(packet_in.data);

    assert_int_equal(packet_out.sender_address, packet_in.sender_address);
    assert_int_equal(packet_out.receiver_address, packet_in.receiver_address);
    assert_int_equal(packet_out.packet_type, packet_in.packet_type);
    assert_int_equal(packet_out.seq_num, packet_in.seq_num);
    assert_int_equal(packet_out.length, packet_in.length);
    assert_int_equal(packet_out.checksum, packet_in.checksum);
    assert_memory_equal(packet_out.data, packet_in.data, packet_out.length);

    free(buffer);
    free(packet_in.data);
}

/* Test twp_recv_raw with valid data and correct checksum */
static void test_twp_recv_raw_success(void **state) {
    (void)state; /* Unused */
    struct data_link_t dl;
    struct packet_t pkt_out, pkt_in;
    uint8_t message[] = "good data";

    // Prepare a packet
    pkt_out.sender_address = 0x11;
    pkt_out.receiver_address = 0x22;
    pkt_out.packet_type = 0xBB;
    pkt_out.seq_num = 10;
    pkt_out.data = message;
    pkt_out.length = sizeof(message);
    twp_set_checksum(&dl, &pkt_out);

    // Pack it
    uint8_t *buffer;
    size_t size;
    twp_pack(&dl, &pkt_out, &buffer, &size);

    // Write to an in-memory stream
    dl.input_stream = fmemopen(buffer, size, "r");
    assert_non_null(dl.input_stream);

    // Receive it
    uint8_t status = twp_recv_raw(&dl, &pkt_in);

    assert_int_equal(status, STATUS_OK);
    assert_int_equal(pkt_in.sender_address, pkt_out.sender_address);
    assert_int_equal(pkt_in.receiver_address, pkt_out.receiver_address);
    assert_int_equal(pkt_in.packet_type, pkt_out.packet_type);
    assert_int_equal(pkt_in.seq_num, pkt_out.seq_num);
    assert_int_equal(pkt_in.length, pkt_out.length);
    assert_memory_equal(pkt_in.data, pkt_out.data, pkt_in.length);
    assert_int_equal(pkt_in.checksum, pkt_out.checksum);

    free(pkt_in.data);
    fclose(dl.input_stream);
    free(buffer);
}

/* Test twp_recv_raw with checksum mismatch */
static void test_twp_recv_raw_checksum_mismatch(void **state) {
    (void)state; /* Unused */
    struct data_link_t dl;
    struct packet_t pkt_out, pkt_in;
    uint8_t message[] = "bad data";

    pkt_out.sender_address = 0x33;
    pkt_out.receiver_address = 0x44;
    pkt_out.packet_type = 0xCC;
    pkt_out.seq_num = 20;
    pkt_out.data = message;
    pkt_out.length = sizeof(message);
    pkt_out.checksum = 0; // Wrong checksum

    uint8_t *buffer;
    size_t size;
    twp_pack(&dl, &pkt_out, &buffer, &size);

    dl.input_stream = fmemopen(buffer, size, "r");
    assert_non_null(dl.input_stream);

    uint8_t status = twp_recv_raw(&dl, &pkt_in);

    assert_int_equal(status, STATUS_ERR_CHECKSUM_MISMATCH);

    fclose(dl.input_stream);
    free(buffer);
}

/* Test twp_recv_wait skips packets for other addresses */
static void test_twp_recv_wait_wrong_address(void **state) {
    (void)state; // unused variable

    struct data_link_t dl;
    dl.node_address = 0x03;
    data_link_init(&dl, NULL, NULL);

    // Packet for address 0x05
    struct packet_t pkt1_out;
    uint8_t msg1[] = "wrong address";
    pkt1_out.sender_address = 0x02;
    pkt1_out.receiver_address = 0x05;
    pkt1_out.packet_type = 0;
    pkt1_out.seq_num = 1;
    pkt1_out.data = msg1;
    pkt1_out.length = sizeof(msg1);
    twp_set_checksum(&dl, &pkt1_out);
    uint8_t *buf1 = NULL;
    size_t size1;
    twp_pack(&dl, &pkt1_out, &buf1, &size1);

    // Packet for address 0x03
    struct packet_t pkt2_out;
    uint8_t msg2[] = "addr match";
    pkt2_out.sender_address = 0x02;
    pkt2_out.receiver_address = 0x03;
    pkt2_out.packet_type = 0;
    pkt2_out.seq_num = 2;
    pkt2_out.data = msg2;
    pkt2_out.length = sizeof(msg2);
    twp_set_checksum(&dl, &pkt2_out);
    uint8_t *buf2 = NULL;
    size_t size2;
    twp_pack(&dl, &pkt2_out, &buf2, &size2);

    // Combine buffers
    size_t total_size = size1 + size2;
    uint8_t *packet_stream = malloc(total_size);
    memcpy(packet_stream, buf1, size1);
    memcpy(packet_stream + size1, buf2, size2);

    FILE *input = fmemopen(packet_stream, total_size, "rb");
    assert_non_null(input);
    dl.input_stream = input;

    struct packet_t packet_in;
    packet_in.data = NULL; // Initialize to NULL for proper cleanup
    uint8_t result = twp_recv_wait(&dl, &packet_in);

    assert_int_equal(result, STATUS_OK);
    assert_int_equal(packet_in.receiver_address, 0x03);
    assert_string_equal((const char *)packet_in.data, "addr match");

    free(packet_in.data);
    fclose(input);
    free(packet_stream);
    free(buf1);
    free(buf2);
}

static void test_twp_validate_success(void **state) {
    (void)state; /* not used */

    struct data_link_t data_link = {.seq_num = 41};
    struct packet_t packet;

    uint8_t data[] = "test data";
    packet.data = data;
    packet.length = sizeof(data);
    packet.seq_num = 42;
    packet.checksum = crc32(0L, (const Bytef *)packet.data, packet.length);

    uint8_t status = twp_validate(&data_link, &packet);

    assert_int_equal(status, STATUS_OK);
    assert_int_equal(data_link.seq_num, 42);
}

static void test_twp_validate_checksum_mismatch(void **state) {
    (void)state; /* not used */

    struct data_link_t data_link = {.seq_num = 41};
    struct packet_t packet;

    uint8_t data[] = "test data";
    packet.data = data;
    packet.length = sizeof(data);
    packet.seq_num = 42;
    packet.checksum = crc32(0L, (const Bytef *)packet.data, packet.length) +
                      1; // Invalid checksum

    uint8_t status = twp_validate(&data_link, &packet);

    assert_int_equal(status, STATUS_ERR_CHECKSUM_MISMATCH);
    assert_int_equal(data_link.seq_num, 41); // Should not be incremented
}

static void test_twp_validate_seq_num_mismatch(void **state) {
    (void)state; /* not used */

    struct data_link_t data_link = {.seq_num = 41};
    struct packet_t packet;

    uint8_t data[] = "test data";
    packet.data = data;
    packet.length = sizeof(data);
    packet.seq_num = 43; // Invalid sequence number
    packet.checksum = crc32(0L, (const Bytef *)packet.data, packet.length);

    uint8_t status = twp_validate(&data_link, &packet);

    assert_int_equal(status, STATUS_ERR_SEQ_NUM_MISMATCH);
    assert_int_equal(data_link.seq_num, 41); // Should not be incremented
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_data_link_init),
        cmocka_unit_test(test_twp_set_seq_num),
        cmocka_unit_test(test_twp_set_checksum),
        cmocka_unit_test(test_twp_pack_unpack),
        cmocka_unit_test(test_twp_recv_raw_success),
        cmocka_unit_test(test_twp_recv_raw_checksum_mismatch),
        cmocka_unit_test(test_twp_recv_wait_wrong_address),
        cmocka_unit_test(test_twp_validate_checksum_mismatch),
        cmocka_unit_test(test_twp_validate_seq_num_mismatch),
        cmocka_unit_test(test_twp_validate_success),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
