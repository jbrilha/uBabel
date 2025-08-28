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

#define EVENT_MESSAGE_SEND 301

#define MICRO_BABEL_SYSTEM_PROTOCOL 0

#define UUID_SIZE 16

// TODO I need to move these elsewhere but not sure where to avoid circular deps
#define UI_EVENT_REC_LORA 1105
typedef struct {
    uint16_t length;
    uint8_t* payload;
    int16_t rssi;
    float snr;
    int32_t freq_err;
} lora_payload_t;
// -------------------------

// === Generic Event Structure ===
typedef struct {
    event_type_t type;
    event_subtype_t subtype;
    uint16_t payload_size; // Size of the payload in bytes
    void* payload; // Pointer to the payload data
    uint8_t reference_counter; //Number of references for this event;
    uint16_t proto_source;
    uint16_t proto_destination;
} event_t;

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

event_t* create_event(event_type_t type, event_subtype_t subtype, void* payload, uint16_t payload_size);
event_t* create_event_with_destination(event_type_t type, event_subtype_t subtype, void* payload, uint16_t payload_size, uint16_t protocol_id);

void free_event(event_t* event);
void print_event(const event_t* event);
bool is_event_type(const event_t* event, event_type_t type);
bool is_event_subtype(const event_t* event, event_subtype_t subtype);
bool is_event_payload_empty(const event_t* event);
void init_event_mutex();


message_t* create_message(uint16_t message_type, uint8_t srcId[UUID_SIZE], uint16_t srcProto, uint8_t destId[UUID_SIZE], uint16_t destProto, uint8_t* payload, uint16_t payload_size);
message_t* create_empty_message(uint16_t message_type, uint8_t srcId[UUID_SIZE], uint16_t srcProto, uint8_t destId[UUID_SIZE], uint16_t destProto);
void free_message(message_t* msg);
uint16_t getFullMessageSize(message_t* msg);
uint16_t computePayloadSize(uint16_t full_message_lenght);

#ifdef __cplusplus
}
#endif

#endif // MICRO_BABEL_EVENT_H
