#ifndef DATA_LINK_H
#define DATA_LINK_H

#include "packets.h"
#include <stdint.h>
#include <stdio.h>

#define BROADCAST_ADDRESS (0xff)
#define LOOPBACK_ADDRESS (0x01)
#define EMPTY_ADDRESS (0x00)

struct data_link_t {
    FILE *input_stream;
    FILE *output_stream;

    uint8_t seq_num;
    uint8_t address;
};

void data_link_init(struct data_link_t *c, FILE *in, FILE *out);
int stream_has_data(FILE *stream);

uint8_t twp_set_seq_num(struct data_link_t *data_link, struct Packet *packet);
uint8_t twp_set_checksum(struct data_link_t *data_link, struct Packet *packet);

uint8_t twp_send(struct data_link_t *data_link, struct Packet *packet);
uint8_t twp_recv_raw(struct data_link_t *data_link, struct Packet *packet);
uint8_t twp_recv_wait(struct data_link_t *data_link, struct Packet *packet);

#endif