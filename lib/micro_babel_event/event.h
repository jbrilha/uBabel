#ifndef MICRO_BABEL_EVENT_H
#define MICRO_BABEL_EVENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "platform.h"

#include "lwip/ip_addr.h"

#define MAX_EVENT_SUBSCRIBERS 8
#define MAX_EVENT_PAYLOAD_SIZE 64

typedef uint16_t event_subtype_t;
typedef uint8_t event_type_t;

#define MAX_EVENT_TYPES 4

#define EVENT_TYPE_MESSAGE 0
#define EVENT_TYPE_TIMER 1
#define EVENT_TYPE_REQUEST 2
#define EVENT_TYPE_NOTIFICATION 3

#define EVENT_SUBTYPE_NETWORK_UP 101
#define EVENT_SUBTYPE_NETWORK_DOWN 102

// === Generic Event Structure ===
typedef struct {
    event_type_t type;
    event_subtype_t subtype;
    uint16_t payload_size; // Size of the payload in bytes
    void* payload; // Pointer to the payload data
    uint8_t reference_counter; //Number of references for this event;
} event_t;

#ifdef __cplusplus
extern "C" {
#endif

event_t* create_event(event_type_t type, event_subtype_t subtype, void* payload, uint16_t payload_size);
void free_event(event_t* event);
void print_event(const event_t* event);
bool is_event_type(const event_t* event, event_type_t type);
bool is_event_subtype(const event_t* event, event_subtype_t subtype);
bool is_event_payload_empty(const event_t* event);
void init_event_mutex();

#ifdef __cplusplus
}
#endif

#endif // MICRO_BABEL_EVENT_H
