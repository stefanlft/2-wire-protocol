#include "../includes/connection.h"
#include "../includes/packets.h"
#include "../includes/statuses.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <zlib.h>

int stream_has_data(FILE *stream) {
    if (feof(stream) || ferror(stream)) {
        return 0; // No data available
    }
    return 1; // Data available
}

uint8_t twp_set_seq_num(struct Connection *conn, struct Packet *packet) {
    packet->seq_num = conn->seq_num++;
    return STATUS_OK;
}

uint8_t twp_set_checksum(struct Connection *conn, struct Packet *packet) {
    packet->checksum = crc32(0L, (const Bytef *)packet->data, packet->length);

    return STATUS_OK;
}

uint8_t twp_send(struct Connection *conn, struct Packet *packet) {
    if (twp_set_seq_num(conn, packet) != STATUS_OK) {
        return STATUS_ERR_NO_SEQ_NUM;
    }

    if (twp_set_checksum(conn, packet) != STATUS_OK) {
        return STATUS_ERR_CHECKSUM;
    }

    fwrite(&packet->packet_type, 1, 1, conn->output_stream);
    fwrite(&packet->seq_num, 1, 1, conn->output_stream);
    fwrite(&packet->length, sizeof(packet->length), 1, conn->output_stream);
    fwrite(packet->data, 1, packet->length, conn->output_stream);
    fwrite(&packet->checksum, 1, 4, conn->output_stream);
    fflush(conn->output_stream);

    return STATUS_OK;
}

uint8_t twp_recv(struct Connection *conn, struct Packet *packet) {
    if (!stream_has_data(conn->input_stream)) {
        return STATUS_ERR_NO_DATA;
    }

    fread((void *)&packet->packet_type, 1, 1, conn->input_stream);
    fread((void *)&packet->seq_num, 1, 1, conn->input_stream);
    fread((void *)&packet->length, sizeof(packet->length), 1,
          conn->input_stream);

    uint8_t *data = malloc(packet->length);
    if (data == NULL) {
        return STATUS_ERR_MALLOC;
    }

    fread((void *)data, 1, packet->length, conn->input_stream);
    packet->data = data;

    fread((void *)&packet->checksum, 1, 4, conn->input_stream);

    uint32_t expected_checksum =
        crc32(0L, (const Bytef *)packet->data, packet->length);

    if (packet->checksum != expected_checksum) {
        return STATUS_ERR_CHECKSUM_MISMATCH;
    }

    return STATUS_OK;
}

uint8_t twp_cobs_encode(struct Connection *conn, const struct Packet *packet,
                        uint8_t **buffer, uint32_t *encoded_size) {
    (void)conn; // Unused parameter

    // Robustly calculate the size of the packet to be encoded.
    uint32_t raw_size = sizeof(packet->packet_type) + sizeof(packet->seq_num) +
                        sizeof(packet->length) + packet->length +
                        sizeof(packet->checksum);

    // Max COBS encoded size: raw_size + 1 for overhead byte + 1 for terminating
    // zero.
    uint32_t encoded_size_max = raw_size + (raw_size / 254) + 2;

    uint8_t *pkt_buff = malloc(raw_size);
    if (!pkt_buff) {
        return STATUS_ERR_MALLOC;
    }

    // Copy packet fields into a contiguous buffer
    uint8_t *current_pos = pkt_buff;

    memcpy(current_pos, &packet->packet_type, sizeof(packet->packet_type));
    current_pos += sizeof(packet->packet_type);

    memcpy(current_pos, &packet->seq_num, sizeof(packet->seq_num));
    current_pos += sizeof(packet->seq_num);

    memcpy(current_pos, &packet->length, sizeof(packet->length));
    current_pos += sizeof(packet->length);

    memcpy(current_pos, packet->data, packet->length);
    current_pos += packet->length;

    memcpy(current_pos, &packet->checksum, sizeof(packet->checksum));

    // Reallocate buffer to hold the COBS encoded data
    uint8_t *temp = realloc(*buffer, encoded_size_max);
    if (!temp) {
        free(pkt_buff);
        return STATUS_ERR_MALLOC;
    }
    *buffer = temp;

    // TODO: implement

    return STATUS_OK;
}

void connection_init(struct Connection *c, FILE *in, FILE *out) {
    c->input_stream = in;
    c->output_stream = out;

    c->seq_num = 0;
}
