#pragma once

// event_types.h

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "lwip/sockets.h"

#ifndef MICRO_BABEL_EVENT_TYPES_H
#define MICRO_BABEL_EVENT_TYPES_H

#define EVENT_REQUEST_DISCOVERY_REGISTER 301
#define EVENT_REQUEST_DISCOVERY_UNREGISTER 302
#define EVENT_NOTIFICATION_DISCOVERY 303

#define EVENT_MESSAGE_DISCOVERY 304

#define MAX_PROTOCOL_NAME_SIZE 38
#define MAX_UDP_PACKET_SIZE 1024//65535

typedef struct register_proto_info {
    char protocol_name[MAX_PROTOCOL_NAME_SIZE + 1]; //Null terminated protocol name
    uint32_t ip;
    uint16_t port;
    QueueHandle_t queue;
} register_proto_info_t;

typedef struct discovery_message {
    struct sockaddr_in sender_addr;
    int messagelenght;
    char buffer[MAX_UDP_PACKET_SIZE]; 
} discovery_message_t;

#ifdef __cplusplus
extern "C" {
#endif  

bool proto_discovery_init(void);

#ifdef __cplusplus
}
#endif  

#endif // MICRO_BABEL_EVENT_TYPES_H