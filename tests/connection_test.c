#include <cmocka.h>
#include "../includes/data_link.h"
#include "../includes/packets.h"
#include "../includes/statuses.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h> // Add this to fix the crc32 and Bytef issues

static void test_connection_init(void **state) {
    (void)state; // unused variable
    FILE *dummy_in = fopen("/dev/null", "r");
    FILE *dummy_out = fopen("/dev/null", "w");

    struct data_link_t data_link;
    data_link_init(&data_link, dummy_in, dummy_out);

    assert_ptr_equal(data_link.input_stream, dummy_in);
    assert_ptr_equal(data_link.output_stream, dummy_out);
    assert_int_equal(data_link.seq_num, 0);

    fclose(dummy_in);
    fclose(dummy_out);
}

static void test_default_send(void **state) {
    (void)state; // unused variable

    FILE *dummy_out = fopen("/dev/null", "w");

    struct data_link_t data_link;
    data_link_init(&data_link, NULL, dummy_out);

    struct Packet packet = {0};
    packet.packet_type = 1;
    packet.seq_num = 42;
    packet.length = 4;
    packet.data = (uint8_t *)"Test";
    packet.checksum = 0;

    // Mock the set_seq_num and set_checkstum functions
    // will_return(data_link.set_seq_num, STATUS_OK); // Mock set_seq_num return value
    // will_return(data_link.set_checkstum, STATUS_OK); // Mock set_checkstum return value

    uint8_t result = twp_send(&data_link, &packet);
    assert_int_equal(result, STATUS_OK);

    fclose(dummy_out);
}

static void test_default_recv(void **state) {
    (void)state; // unused variable

    // Open the real test file
    // dump_file_in_hex("test_files/test_default_recv.bin");

    FILE *dummy_in = fopen("../test_files/test_default_recv.bin", "rb");  // Open the sample file in binary read mode
    if (!dummy_in) {
        perror("Failed to open test_default_recv.bin");
        exit(EXIT_FAILURE);  // Make sure the test fails if the file can't be opened
    }

    rewind(dummy_in);

    struct data_link_t data_link;
    data_link.node_address = 1;
    data_link_init(&data_link, dummy_in, NULL);  // Initialize data_link_t with the real file stream

    struct Packet packet;

    uint8_t result = twp_recv_wait(&data_link, &packet);
    assert_int_equal(result, STATUS_OK);  // Ensure the function returns success
    assert_int_equal(packet.packet_type, 0);  // Verify expected packet type
    assert_int_equal(packet.seq_num, 0);  // Verify expected sequence number
    assert_int_equal(packet.length, 0x0c);  // Verify expected packet length
    assert_string_equal((const char *)packet.data, "HELLO THERE!");  // Verify packet data content

    free(packet.data);  // Free allocated memory for the packet data
    fclose(dummy_in);  // Close the file after use
}



static void test_default_set_checksum(void **state) {
    (void)state; // unused variable

    struct data_link_t data_link;
    data_link_init(&data_link, NULL, NULL);

    struct Packet packet = {0};
    packet.packet_type = 1;
    packet.seq_num = 42;
    packet.length = 4;
    packet.data = (uint8_t *)"Test";

    uint8_t result = twp_set_checksum(&data_link, &packet);
    assert_int_equal(result, STATUS_OK);
    assert_int_equal(packet.checksum, crc32(0L, (const Bytef *)"Test", 4));
}

static void test_twp_set_seq_num(void **state) {
    (void)state; // unused variable

    struct data_link_t data_link;
    data_link_init(&data_link, NULL, NULL);
    assert_int_equal(data_link.seq_num, 0);

    struct Packet packet = {0};

    uint8_t result = twp_set_seq_num(&data_link, &packet);
    assert_int_equal(result, STATUS_OK);
    assert_int_equal(packet.seq_num, 0);
    assert_int_equal(data_link.seq_num, 1);

    result = twp_set_seq_num(&data_link, &packet);
    assert_int_equal(result, STATUS_OK);
    assert_int_equal(packet.seq_num, 1);
    assert_int_equal(data_link.seq_num, 2);
}

static void test_twp_recv_raw_checksum_mismatch(void **state) {
    (void)state; // unused variable

    // This is a valid packet for "HELLO THERE!" but with the last byte of checksum flipped
    // Correct checksum is 0x9639847c
    uint8_t bad_packet[] = {
        0x00, 0x01, 0x00, 0x00, 0x0c, 'H', 'E', 'L', 'L', 'O', ' ',
        'T', 'H', 'E', 'R', 'E', '!', 0x7c, 0x84, 0x39, 0x97 // Wrong checksum
    };

    FILE *input = fmemopen(bad_packet, sizeof(bad_packet), "rb");
    assert_non_null(input);

    struct data_link_t data_link;
    data_link_init(&data_link, input, NULL);

    struct Packet packet;
    uint8_t result = twp_recv_raw(&data_link, &packet);

    assert_int_equal(result, STATUS_ERR_CHECKSUM_MISMATCH);

    fclose(input);
}

static void test_twp_recv_wait_wrong_address(void **state) {
    (void)state; // unused variable

    // Packet for address 0x05, then a packet for address 0x03
    unsigned char packet_stream[] = {
        0x02, 0x05, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 
        0x61, 0x64, 0x64, 0x72, 0x20, 0x6d, 0x69, 0x73, 
        0x73, 0x6d, 0x61, 0x74, 0x63, 0x68, 0x67, 0x42, 
        0xa3, 0x42,
        
        0x02, 0x03, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 
        0x61, 0x64, 0x64, 0x72, 0x20, 0x6d, 0x61, 0x74, 
        0x63, 0x68, 0x00, 0xa3, 0xc2, 0x46, 0x11, 
    };

    FILE *input = fmemopen(packet_stream, sizeof(packet_stream), "rb");
    assert_non_null(input);

    struct data_link_t data_link;
    data_link.node_address = 0x03;
    data_link_init(&data_link, input, NULL);

    struct Packet packet;
    uint8_t result = twp_recv_wait(&data_link, &packet);

    assert_int_equal(result, STATUS_OK);
    assert_int_equal(packet.receiver_address, 0x03);
    assert_string_equal((const char *)packet.data, "addr match");

    free(packet.data);
    fclose(input);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_connection_init),
        cmocka_unit_test(test_default_send),
        cmocka_unit_test(test_default_recv),
        cmocka_unit_test(test_default_set_checksum),
        cmocka_unit_test(test_twp_set_seq_num),
        cmocka_unit_test(test_twp_recv_raw_checksum_mismatch),
        cmocka_unit_test(test_twp_recv_wait_wrong_address),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
