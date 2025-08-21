#pragma once

#include <stdint.h>
#include "comm_manager.h"

typedef struct {
    uint16_t message_type;
    uint8_t sourceId[UUID_SIZE];
    uint16_t sourceProto;
    uint8_t destId[UUID_SIZE];
    uint16_t destProto;
    uint8_t* payload;
    uint16_t payload_size;
} message_t;

#ifdef __cplusplus
extern "C" {
#endif  

message_t* create_message(uint16_t message_type, uint8_t srcId[UUID_SIZE], uint16_t srcProto, uint8_t destId[UUID_SIZE], uint16_t destProto, uint8_t* payload, uint16_t payload_size);

message_t* create_empty_message(uint16_t message_type, uint8_t srcId[UUID_SIZE], uint16_t srcProto, uint8_t destId[UUID_SIZE], uint16_t destProto);

void free_message(message_t* msg);

uint16_t getFullMessageSize(message_t* msg);

uint16_t computePayloadSize(uint16_t full_message_lenght);


#ifdef __cplusplus
}
#endif  