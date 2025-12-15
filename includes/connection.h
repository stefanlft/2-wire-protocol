#ifndef CONNECTION_H
#define CONNECTION_H

#include "packets.h"
#include <stdint.h>
#include <stdio.h>

struct __attribute__((packed)) Connection {
    FILE *input_stream;
    FILE *output_stream;

    uint8_t seq_num;
};

void connection_init(struct Connection *c, FILE *in, FILE *out);
int stream_has_data(FILE *stream);

uint8_t twp_set_seq_num(struct Connection *conn, struct Packet *packet);
uint8_t twp_set_checksum(struct Connection *conn, struct Packet *packet);

uint8_t twp_cobs_encode(struct Connection *conn, const struct Packet *packet,
                        uint8_t **buffer, uint32_t *encoded_size);

uint8_t twp_send(struct Connection *conn, struct Packet *packet);
uint8_t twp_recv(struct Connection *conn, struct Packet *packet);

#endif