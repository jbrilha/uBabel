#include "message.h"

message_t* create_message(uint16_t message_type, uint8_t srcId[UUID_SIZE], uint16_t srcProto, uint8_t destId[UUID_SIZE], uint16_t destProto, uint8_t* payload, uint16_t payload_size) {
    message_t* msg = (message_t*) malloc(sizeof(message_t));
    
    if(msg != NULL) {
        msg->message_type = message_type;
        memcpy(msg->sourceId, srcId, UUID_SIZE);
        memcpy(msg->destId, destId, UUID_SIZE);
        msg->sourceProto = srcProto;
        msg->destProto = destProto;
        msg->payload = payload;
        msg->payload_size = payload_size;
    }

    return msg;
}

message_t* create_empty_message(uint16_t message_type, uint8_t srcId[UUID_SIZE], uint16_t srcProto, uint8_t destId[UUID_SIZE], uint16_t destProto) {
    message_t* msg = (message_t*) malloc(sizeof(message_t));
    
    if(msg != NULL) {
        msg->message_type = message_type;
        memcpy(msg->sourceId, srcId, UUID_SIZE);
        memcpy(msg->destId, destId, UUID_SIZE);
        msg->sourceProto = srcProto;
        msg->destProto = destProto;
        msg->payload = NULL;
        msg->payload_size = 0;
    }

    return msg;
}

void free_message(message_t* msg) {
    if(msg->payload != NULL)
        free(msg->payload);
    free(msg);
}

uint16_t getFullMessageSize(message_t* msg) {
    return msg->payload_size + 16 * 4 + UUID_SIZE * 2;
}

uint16_t computePayloadSize(uint16_t full_message_lenght) {
    return full_message_lenght - (16 * 4 + UUID_SIZE * 2);
}