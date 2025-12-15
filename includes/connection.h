#ifndef CONNECTION_H
#define CONNECTION_H

#include "packets.h"
#include <stdint.h>
#include <stdio.h>

struct Connection {
    FILE *input_stream;
    FILE *output_stream;

    uint8_t seq_num;
};

void connection_init(struct Connection *c, FILE *in, FILE *out);
int stream_has_data(FILE *stream);
uint8_t twp_set_seq_num(struct Connection *self, struct Packet *packet);
uint8_t twp_set_checksum(struct Connection *self, struct Packet *packet);
uint8_t twp_send(struct Connection *self, struct Packet *packet);
uint8_t twp_recv(struct Connection *self, struct Packet *packet);

#endif