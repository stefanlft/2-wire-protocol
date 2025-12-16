#ifndef CONNECTION_H
#define CONNECTION_H

#include "packets.h"
#include <stdint.h>
#include <stdio.h>

#define BROADCAST_ADDRESS (0xff)
#define LOOPBACK_ADDRESS (0x01)
#define EMPTY_ADDRESS (0x00)

struct Connection {
    FILE *input_stream;
    FILE *output_stream;

    uint8_t seq_num;
    uint8_t address;
};

void connection_init(struct Connection *c, FILE *in, FILE *out);
int stream_has_data(FILE *stream);

uint8_t twp_set_seq_num(struct Connection *conn, struct Packet *packet);
uint8_t twp_set_checksum(struct Connection *conn, struct Packet *packet);

uint8_t twp_send(struct Connection *conn, struct Packet *packet);
uint8_t twp_recv_raw(struct Connection *conn, struct Packet *packet);
uint8_t twp_recv_wait(struct Connection *conn, struct Packet *packet);

#endif