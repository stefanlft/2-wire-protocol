#include "../includes/connection.h"
#include "../includes/packets.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

int main(int argc, char **argv) {
    FILE *input_stream = fopen("../test_files/input_stream.bin", "r");
    FILE *output_stream = fopen("../test_files/output_stream.bin", "w");

    if (input_stream == NULL || output_stream == NULL) {
        perror("Could not open files!");
        return 1;
    }

    struct Connection conn;
    connection_init(&conn, input_stream, output_stream);

    struct Packet packet_out;
    packet_out.packet_type = 0;
    uint8_t message[] = "HELLO THERE!";
    packet_out.data = message;
    packet_out.length = strlen((const char *)message);

    printf("send: %d\n", twp_send(&conn, &packet_out));

    struct Packet packet_in;
    printf("recv: %d\n", twp_recv(&conn, &packet_in));

    printf("%s\n", packet_in.data);

    fclose(input_stream);
    fclose(output_stream);

    return 0;
}