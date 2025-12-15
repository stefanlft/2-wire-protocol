#include "../includes/connection.h"
#include "../includes/packets.h"
#include "../includes/statuses.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>
#include <zlib.h>

int stream_has_data(FILE *stream) {
    if (feof(stream) || ferror(stream)) {
        return 0; // No data available
    }
    return 1; // Data available
}

uint8_t twp_set_seq_num(struct Connection *self, struct Packet *packet) {
    packet->seq_num = self->seq_num++;
    return STATUS_OK;
}

uint8_t twp_send(struct Connection *self, struct Packet *packet) {
    if (twp_set_seq_num(self, packet) != STATUS_OK) {
        return STATUS_ERR_NO_SEQ_NUM;
    }

    if (twp_set_checksum(self, packet) != STATUS_OK) {
        return STATUS_ERR_CHECKSUM;
    }

    fwrite(&packet->packet_type, 1, 1, self->output_stream);
    fwrite(&packet->seq_num, 1, 1, self->output_stream);
    fwrite(&packet->length, sizeof(packet->length), 1, self->output_stream);
    fwrite(packet->data, 1, packet->length, self->output_stream);
    fwrite(&packet->checksum, 1, 4, self->output_stream);
    fflush(self->output_stream);

    return STATUS_OK;
}

uint8_t twp_recv(struct Connection *self, struct Packet *packet) {
    if (!stream_has_data(self->input_stream)) {
        return STATUS_ERR_NO_DATA;
    }

    fread((void *)&packet->packet_type, 1, 1, self->input_stream);
    fread((void *)&packet->seq_num, 1, 1, self->input_stream);
    fread((void *)&packet->length, sizeof(packet->length), 1,
          self->input_stream);

    uint8_t *data = malloc(packet->length);
    if (data == NULL) {
        return STATUS_ERR_MALLOC;
    }

    fread((void *)data, 1, packet->length, self->input_stream);
    packet->data = data;

    fread((void *)&packet->checksum, 1, 4, self->input_stream);

    uint32_t expected_checksum =
        crc32(0L, (const Bytef *)packet->data, packet->length);

    if (packet->checksum != expected_checksum) {
        return STATUS_ERR_CHECKSUM_MISMATCH;
    }

    return STATUS_OK;
}

uint8_t twp_set_checksum(struct Connection *self, struct Packet *packet) {
    packet->checksum = crc32(0L, (const Bytef *)packet->data, packet->length);

    return STATUS_OK;
}

void connection_init(struct Connection *c, FILE *in, FILE *out) {
    c->input_stream = in;
    c->output_stream = out;

    c->seq_num = 0;
}