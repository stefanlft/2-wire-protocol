#ifndef CONNECTION_H
#define CONNECTION_H

#include "packets.h"
#include <stdio.h>
#include <stdint.h>

struct Connection {
    FILE *input_stream;
    FILE *output_stream;

    uint8_t seq_num;
    
    uint8_t (*set_seq_num) (struct Connection *self, struct Packet *packet);
    uint8_t (*set_checkstum) (struct Connection *self, struct Packet *packet);
    uint8_t (*send) (struct Connection *self, struct Packet *packet);
    uint8_t (*recv) (struct Connection *self, struct Packet *packet);
};

void connection_init(struct Connection *c, FILE *in, FILE *out);

#endif