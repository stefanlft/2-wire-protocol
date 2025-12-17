#include "../includes/data_link.h"
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

uint8_t twp_set_seq_num(struct data_link_t *data_link,
                        struct packet_t *packet) {
    packet->seq_num = data_link->seq_num++;
    return STATUS_OK;
}

uint8_t twp_set_checksum(struct data_link_t *data_link,
                         struct packet_t *packet) {
    packet->checksum = crc32(0L, (const Bytef *)packet->data, packet->length);

    return STATUS_OK;
}

uint8_t twp_send(struct data_link_t *data_link, struct packet_t *packet) {
    if (twp_set_seq_num(data_link, packet) != STATUS_OK) {
        return STATUS_ERR_NO_SEQ_NUM;
    }

    if (twp_set_checksum(data_link, packet) != STATUS_OK) {
        return STATUS_ERR_CHECKSUM;
    }

    packet->sender_address = data_link->node_address;

    uint8_t *buffer;
    size_t size;

    twp_pack(data_link, packet, &buffer, &size);

    fwrite(buffer, size, 1, data_link->output_stream);
    fflush(data_link->output_stream);

    free(buffer);

    return STATUS_OK;
}

uint8_t twp_recv_raw(struct data_link_t *data_link, struct packet_t *packet) {
    if (!stream_has_data(data_link->input_stream)) {
        return STATUS_ERR_NO_DATA;
    }

    fread(&packet->sender_address, sizeof(packet->sender_address), 1,
          data_link->input_stream);
    fread(&packet->receiver_address, sizeof(packet->receiver_address), 1,
          data_link->input_stream);
    fread(&packet->packet_type, sizeof(packet->packet_type), 1,
          data_link->input_stream);
    fread(&packet->seq_num, sizeof(packet->seq_num), 1,
          data_link->input_stream);
    fread(&packet->length, sizeof(packet->length), 1, data_link->input_stream);

    uint8_t *data = malloc(packet->length);
    if (data == NULL) {
        return STATUS_ERR_MALLOC;
    }

    fread((void *)data, 1, packet->length, data_link->input_stream);
    packet->data = data;

    fread((void *)&packet->checksum, sizeof(packet->checksum), 1,
          data_link->input_stream);

    uint32_t expected_checksum =
        crc32(0L, (const Bytef *)packet->data, packet->length);

    if (packet->checksum != expected_checksum) {
        free(packet->data);
        return STATUS_ERR_CHECKSUM_MISMATCH;
    }

    return STATUS_OK;
}

static uint8_t check_addr(struct data_link_t *data_link, struct packet_t *pkt) {
    if (pkt->receiver_address == BROADCAST_ADDRESS ||
        pkt->receiver_address == LOOPBACK_ADDRESS ||
        pkt->receiver_address == data_link->node_address) {
        return 1;
    }
    return 0;
}

uint8_t twp_recv_wait(struct data_link_t *data_link, struct packet_t *packet) {
    uint8_t err = STATUS_OK;
    do {
        err = twp_recv_raw(data_link, packet);
        if (err != STATUS_OK) {
            return err;
        }
    } while (!check_addr(data_link, packet));

    return STATUS_OK;
}

uint8_t twp_pack(struct data_link_t *data_link, struct packet_t *packet,
                 uint8_t **buffer, size_t *size) {
    *size = sizeof(struct packet_t) + packet->length - sizeof(packet->data);
    *buffer = malloc(*size);
    if (*buffer == NULL) {
        return STATUS_ERR_MALLOC;
    }

    unsigned pos = 0;

    memcpy(*buffer + pos, &packet->sender_address,
           sizeof(packet->sender_address));
    pos += sizeof(packet->sender_address);

    memcpy(*buffer + pos, &packet->receiver_address,
           sizeof(packet->receiver_address));
    pos += sizeof(packet->receiver_address);

    memcpy(*buffer + pos, &packet->packet_type, sizeof(packet->packet_type));
    pos += sizeof(packet->packet_type);

    memcpy(*buffer + pos, &packet->seq_num, sizeof(packet->seq_num));
    pos += sizeof(packet->seq_num);

    memcpy(*buffer + pos, &packet->length, sizeof(packet->length));
    pos += sizeof(packet->length);

    memcpy(*buffer + pos, packet->data, packet->length);
    pos += packet->length;

    memcpy(*buffer + pos, &packet->checksum, sizeof(packet->checksum));
    pos += sizeof(packet->checksum);

    return STATUS_OK;
}

uint8_t twp_unpack(struct data_link_t *data_link, uint8_t *buffer, size_t size,
                   struct packet_t *packet) {
    if (buffer == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    unsigned pos = 0;

    memcpy(&packet->sender_address, buffer + pos,
           sizeof(packet->sender_address));
    pos += sizeof(packet->sender_address);

    memcpy(&packet->receiver_address, buffer + pos,
           sizeof(packet->receiver_address));
    pos += sizeof(packet->receiver_address);

    memcpy(&packet->packet_type, buffer + pos, sizeof(packet->packet_type));
    pos += sizeof(packet->packet_type);

    memcpy(&packet->seq_num, buffer + pos, sizeof(packet->seq_num));
    pos += sizeof(packet->seq_num);

    memcpy(&packet->length, buffer + pos, sizeof(packet->length));
    pos += sizeof(packet->length);

    packet->data = malloc(packet->length);
    if (packet->data == NULL) {
        return STATUS_ERR_MALLOC;
    }

    memcpy(packet->data, buffer + pos, packet->length);
    pos += packet->length;

    memcpy(&packet->checksum, buffer + pos, sizeof(packet->checksum));
    pos += sizeof(packet->checksum);

    return STATUS_OK;
}

void data_link_init(struct data_link_t *c, FILE *in, FILE *out) {
    c->input_stream = in;
    c->output_stream = out;

    c->seq_num = 0;
}
