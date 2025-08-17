#pragma once

// event_types.h

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "lwip/sockets.h"
#include "event.h"

#ifndef MICRO_BABEL_EVENT_TYPES_H
#define MICRO_BABEL_EVENT_TYPES_H

#define EVENT_REQUEST_OPEN_CONNECTION 301
#define EVENT_REQUEST_CLOSE_CONNECTION 302
#define EVENT_REQUEST_ADD_CONNECTION 303

#define EVENT_NOTIFICATION_NODE_DISCOVERED 301
#define EVENT_NOTIFICATION_NODE_CONNECTED 302
#define EVENT_NOTIFICATION_NODE_SUSPECTED 303
#define EVENT_NOTIFICATION_NODE_FAILED 304

#define UUID_SIZE 16

typedef struct {
    uint8_t id[UUID_SIZE];
    ip4_addr_t address;
} add_connection_request_t;

#ifdef __cplusplus
extern "C" {
#endif  

bool comm_manager_init(void);

bool open_conection(const uint8_t* destination_id, uint16_t proto_id);
void close_conection(const uint8_t* destination_id, uint16_t proto_id);

bool send_message(event_t* msg, const uint8_t* destination_id);
bool send_message_multiple(event_t* msg, const uint8_t** destinations, uint8_t n_destinations); 

#ifdef __cplusplus
}
#endif  

#endif // MICRO_BABEL_EVENT_TYPES_H