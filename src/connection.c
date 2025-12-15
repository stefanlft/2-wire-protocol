#include "connection.h"
#include "packets.h"
#include "statuses.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>
#include <zlib.h>

static int stream_has_data(FILE *stream) {
    int fd = fileno(stream);

    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);

    struct timeval timeout = {0, 0};  // non-blocking

    int rv = select(fd + 1, &set, NULL, NULL, &timeout);
    if (rv == -1)
        return -1;      // error
    if (rv == 0)
        return 0;       // no data available

    return 1;           // data available to read
}


static uint8_t default_set_seq_num(struct Connection *self, struct Packet *packet) {
    packet->seq_num = self->seq_num++;
    return STATUS_OK;
}

static uint8_t default_send(struct Connection *self, struct Packet *packet) {
    if (self->set_seq_num(self, packet) != STATUS_OK) {
        return STATUS_ERR_NO_SEQ_NUM;
    }

    if (self->set_checkstum(self, packet) != STATUS_OK) {
        return STATUS_ERR_CHECKSUM;
    }

    fwrite(&packet->packet_type,1, 1, self->output_stream);
    fwrite(&packet->seq_num,    1, 1, self->output_stream);
    fwrite(&packet->length,     1, 1, self->output_stream);
    fwrite(packet->data,        1, packet->length, self->output_stream);
    fwrite(&packet->checksum,   1, 4, self->output_stream);
    fflush(self->output_stream);

    return STATUS_OK;
}

static uint8_t default_recv(struct Connection *self, struct Packet *packet) {
    if (!stream_has_data(self->input_stream)) {
        return STATUS_ERR_NO_DATA;
    }

    fread((void *)&packet->packet_type, 1, 1, self->input_stream);
    fread((void *)&packet->seq_num, 1, 1, self->input_stream);
    fread((void *)&packet->length, 1, 1, self->input_stream);

    uint8_t *data = malloc(packet->length);

    fread((void *)&data, 1, packet->length, self->input_stream);
    packet->data = data;

    fread((void *)&packet->checksum, 1, 4, self->input_stream);


    return STATUS_OK;
}

static uint8_t default_set_checksum(struct Connection *self, struct Packet *packet) {
    packet->checksum = crc32(0L, (const Bytef *)packet->data, packet->length);
    
    return STATUS_OK;
}

void connection_init(struct Connection *c, FILE *in, FILE *out) {
    c->input_stream = in;
    c->output_stream = out;

    c->seq_num = 0;

    c->set_seq_num   = default_set_seq_num;
    c->send          = default_send;
    c->recv          = default_recv;
    c->set_checkstum = default_set_checksum;
}