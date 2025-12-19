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
    } // TODO: This function is not robust. It should check if there's actual
      // data to read, not just if the stream is not at EOF or in an error
      // state. For example, using select() or fcntl() for non-blocking I/O
      // would be better.
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
        return STATUS_ERR_SETTING_CHECKSUM;
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

    // Read fixed-size header fields
    if (fread(&packet->sender_address, sizeof(packet->sender_address), 1,
              data_link->input_stream) != 1 ||
        fread(&packet->receiver_address, sizeof(packet->receiver_address), 1,
              data_link->input_stream) != 1 ||
        fread(&packet->packet_type, sizeof(packet->packet_type), 1,
              data_link->input_stream) != 1 ||
        fread(&packet->seq_num, sizeof(packet->seq_num), 1,
              data_link->input_stream) != 1 ||
        fread(&packet->length, sizeof(packet->length), 1,
              data_link->input_stream) != 1) {
        // Handle read error or EOF before reading full header
        return STATUS_ERR_READ_FAILED; // Or a more specific error
    }

    uint8_t *data = malloc(packet->length);
    if (data == NULL) {
        return STATUS_ERR_MALLOC;
    }

    if (fread((void *)data, 1, packet->length, data_link->input_stream) !=
        packet->length) {
        free(data);
        return STATUS_ERR_READ_FAILED; // Or a more specific error
    }
    packet->data = data;

    if (fread((void *)&packet->checksum, sizeof(packet->checksum), 1,
              data_link->input_stream) != 1) {
        free(packet->data);
        return STATUS_ERR_READ_FAILED; // Or a more specific error
    }

    uint32_t expected_checksum =
        crc32(0L, (const Bytef *)packet->data, packet->length);

    // It's good practice to free packet->data if the checksum fails,
    // as the caller might not expect to free it if the function returns an
    // error. This prevents memory leaks in error paths.

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
    // Calculate the total size needed for the buffer.
    // This should be the sum of all individual member sizes, plus the actual
    // data length. sizeof(struct packet_t) is not reliable here as it includes
    // the pointer 'data' and might have padding.
    *size = sizeof(packet->sender_address) + sizeof(packet->receiver_address) +
            sizeof(packet->packet_type) + sizeof(packet->seq_num) +
            sizeof(packet->length) + packet->length + sizeof(packet->checksum);
    *buffer = malloc(*size);
    if (*buffer == NULL) {
        return STATUS_ERR_MALLOC;
    }

    unsigned pos = 0;

    // Use a helper macro or function for memcpy to reduce repetition and
    // potential errors For example: #define COPY_FIELD(field) memcpy(*buffer +
    // pos, &packet->field, sizeof(packet->field)); pos +=
    // sizeof(packet->field);

    memcpy(*buffer + pos, &packet->sender_address,
           sizeof(packet->sender_address));
    pos += sizeof(packet->sender_address);

    memcpy(*buffer + pos, &packet->receiver_address,
           sizeof(packet->receiver_address));
    pos += sizeof(packet->receiver_address); // Consider using a consistent
                                             // style for incrementing pos

    memcpy(*buffer + pos, &packet->packet_type, sizeof(packet->packet_type));
    pos += sizeof(packet->packet_type);

    memcpy(*buffer + pos, &packet->seq_num, sizeof(packet->seq_num));
    pos += sizeof(packet->seq_num); // Consistent style

    memcpy(*buffer + pos, &packet->length, sizeof(packet->length));
    pos += sizeof(packet->length); // Consistent style

    memcpy(*buffer + pos, packet->data, packet->length);
    pos += packet->length;

    memcpy(*buffer + pos, &packet->checksum, sizeof(packet->checksum));
    pos += sizeof(packet->checksum);

    // It's good practice to assert that 'pos' equals 'size' at the end
    // to ensure all data has been copied and the size calculation was correct.
    // assert(pos == *size);

    return STATUS_OK;
}

uint8_t twp_unpack(struct data_link_t *data_link, uint8_t *buffer, size_t size,
                   struct packet_t *packet) {
    if (buffer == NULL) {
        return STATUS_ERR_NULL_PTR;
    }

    // The twp_unpack function currently assumes the buffer contains a complete
    // packet. It should ideally perform checks to ensure 'size' is large enough
    // to contain at least the fixed-size header before attempting to read
    // individual fields. Also, after reading packet->length, it should check if
    // 'size' is sufficient for the data payload + checksum.

    unsigned pos = 0;

    memcpy(&packet->sender_address, buffer + pos,
           sizeof(packet->sender_address));
    pos += sizeof(packet->sender_address); // Consistent style

    memcpy(&packet->receiver_address, buffer + pos,
           sizeof(packet->receiver_address));
    pos += sizeof(packet->receiver_address); // Consistent style

    memcpy(&packet->packet_type, buffer + pos, sizeof(packet->packet_type));
    pos += sizeof(packet->packet_type);

    memcpy(&packet->seq_num, buffer + pos, sizeof(packet->seq_num));
    pos += sizeof(packet->seq_num); // Consistent style

    memcpy(&packet->length, buffer + pos, sizeof(packet->length));
    pos += sizeof(packet->length); // Consistent style

    packet->data = malloc(packet->length);
    if (packet->data == NULL) {
        return STATUS_ERR_MALLOC;
        // Consider what happens if previous fields were copied to 'packet' but
        // malloc fails. The 'packet' structure might be in a partially
        // initialized state.
    }

    memcpy(packet->data, buffer + pos, packet->length);
    pos += packet->length;

    memcpy(&packet->checksum, buffer + pos, sizeof(packet->checksum));
    pos += sizeof(packet->checksum);

    // Similar to twp_pack, assert(pos == size) could be useful here
    // to ensure the entire buffer was consumed as expected.

    return STATUS_OK;
}

uint8_t twp_validate(struct data_link_t *data_link, struct packet_t *packet) {
    if (packet->checksum !=
        crc32(0L, (const Bytef *)packet->data, packet->length)) {
        return STATUS_ERR_CHECKSUM_MISMATCH;
    }
    if (packet->seq_num != data_link->seq_num + 1) {
        return STATUS_ERR_SEQ_NUM_MISMATCH;
    }

    ++data_link->seq_num;

    return STATUS_OK;
}

void data_link_init(struct data_link_t *c, FILE *in, FILE *out) {
    c->input_stream = in;
    c->output_stream = out;

    c->seq_num = 0;
}
