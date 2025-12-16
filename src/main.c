#include "../includes/data_link.h"
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

    struct data_link_t data_link = {.node_address = 0x02,
                                    .input_stream = input_stream,
                                    .output_stream = output_stream,
                                    .seq_num = 0};

    struct Packet packet_out;
    packet_out.receiver_address = 0x03;
    packet_out.packet_type = 0;
    uint8_t message[] = "addr match";
    packet_out.data = message;
    packet_out.length = sizeof(message);

    printf("send: %d\n", twp_send(&data_link, &packet_out));

    struct Packet packet_in;
    printf("recv: %d\n", twp_recv_wait(&data_link, &packet_in));

    printf("%s\n", packet_in.data);

    fclose(input_stream);
    fclose(output_stream);

    return 0;
}