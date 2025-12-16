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

    packet->sender_address = conn->address;

    fwrite(&packet->sender_address, sizeof(packet->sender_address), 1,
           conn->output_stream);
    fwrite(&packet->receiver_address, sizeof(packet->receiver_address), 1,
           conn->output_stream);

    fwrite(&packet->packet_type, sizeof(packet->packet_type), 1,
           conn->output_stream);
    fwrite(&packet->seq_num, sizeof(packet->seq_num), 1, conn->output_stream);
    fwrite(&packet->length, sizeof(packet->length), 1, conn->output_stream);
    fwrite(packet->data, 1, packet->length, conn->output_stream);
    fwrite(&packet->checksum, sizeof(packet->checksum), 1, conn->output_stream);
    fflush(conn->output_stream);

    return STATUS_OK;
}

uint8_t twp_recv_raw(struct Connection *conn, struct Packet *packet) {
    if (!stream_has_data(conn->input_stream)) {
        return STATUS_ERR_NO_DATA;
    }

    fread(&packet->sender_address, sizeof(packet->sender_address), 1,
          conn->input_stream);
    fread(&packet->receiver_address, sizeof(packet->receiver_address), 1,
          conn->input_stream);
    fread(&packet->packet_type, sizeof(packet->packet_type), 1,
          conn->input_stream);
    fread(&packet->seq_num, sizeof(packet->seq_num), 1, conn->input_stream);
    fread(&packet->length, sizeof(packet->length), 1, conn->input_stream);

    uint8_t *data = malloc(packet->length);
    if (data == NULL) {
        return STATUS_ERR_MALLOC;
    }

    fread((void *)data, 1, packet->length, conn->input_stream);
    packet->data = data;

    fread((void *)&packet->checksum, sizeof(packet->checksum), 1,
          conn->input_stream);

    uint32_t expected_checksum =
        crc32(0L, (const Bytef *)packet->data, packet->length);

    if (packet->checksum != expected_checksum) {
        return STATUS_ERR_CHECKSUM_MISMATCH;
    }

    return STATUS_OK;
}

uint8_t twp_recv_wait(struct Connection *conn, struct Packet *packet) {
    uint8_t err = STATUS_OK;
    do {
        err = twp_recv_raw(conn, packet);
        if (err != STATUS_OK) {
            return err;
        }
    } while (packet->receiver_address != conn->address &&
             packet->receiver_address != BROADCAST_ADDRESS &&
             !(packet->receiver_address == LOOPBACK_ADDRESS &&
               packet->sender_address == conn->address));

    return STATUS_OK;
}

void connection_init(struct Connection *c, FILE *in, FILE *out) {
    c->input_stream = in;
    c->output_stream = out;

    c->seq_num = 0;
}
