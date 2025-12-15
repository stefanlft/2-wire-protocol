#ifndef PACKETS_H
#define PACKETS_H

#include <stdint.h>

struct Packet {
    uint8_t packet_type;   // Packet type identifier
    uint8_t seq_num;       // Sequence number for packet ordering
    uint8_t length;       // Length of the payload
    uint8_t *data;        // Actual data (payload)
    uint32_t checksum;    // CRC-32 checksum of data
};

struct DataPacket {
    uint8_t packetType;     // Packet type identifier ( 0x00 )
    uint8_t seqNum;         // Sequence number for packet ordering
    uint8_t length;         // Length of the payload
    uint8_t *data;          // Actual data (payload)
    uint32_t checksum;      // Checksum for error detection
};

struct AckPacket {
    uint8_t packetType; // Set to ACK ( 0x01 )
    uint8_t seqNum;     // Sequence number of the received Data Packet
    uint16_t checksum;  // Checksum for error detection
};

struct NackPacket {
    uint8_t packetType;      // Set to NACK ( 0x03 )
   uint8_t seqNum;       // Sequence number of the Data Packet that requires retransmission
    uint8_t reasonCode;      // Reason for retransmission 
    uint16_t checksum;       // Checksum for error detection
};

struct HeartbeatPacket {
    uint8_t packetType;  // Set to Heartbeat ( 0x04 )
    uint32_t timestamp;  // Timestamp when the heartbeat was sent
    uint16_t checksum;   // Checksum for error detection
};

struct ErrorPacket {
    uint8_t packetType;    // Set to Error ( 0x05)
    uint8_t errorCode;     // Error code (e.g., 0x01 for checksum failure)
    uint8_t *errorMessage; // Optional error message (e.g., "Checksum Mismatch")
    uint16_t checksum;     // Checksum for error detection
};

struct ConfigPacket {
    uint8_t packetType;       // Set to Config (e.g., 0x06)
    uint8_t configID;         // ID or type of configuration being set (e.g., mode, threshold)
    uint8_t length;           // Length of the configuration data
    uint32_t configData;      // Configuration data (e.g., mode setting, threshold value)
    uint16_t checksum;        // Checksum for error detection
};

#endif
