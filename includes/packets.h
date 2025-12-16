#ifndef PACKETS_H
#define PACKETS_H

#include <stdint.h>

struct __attribute__((packed)) Packet {
    uint8_t sender_address;   // Address of the sender
    uint8_t receiver_address; // Address of the sender
    uint8_t packet_type;      // Packet type identifier
    uint8_t seq_num;          // Sequence number for packet ordering
    uint32_t length;          // Length of the payload
    uint8_t *data;            // Actual data (payload)
    uint32_t checksum;        // CRC-32 checksum of data
};

#endif
