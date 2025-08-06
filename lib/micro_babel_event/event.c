#include "event.h"

event_t* create_event(event_type_t type, event_subtype_t subtype, void* payload, uint16_t payload_size) {
    event_t* event = malloc(sizeof(event_t));
    if (!event) {
        printf("Failed to allocate memory for event\n");
        return NULL;
    }
    
    event->type = type;
    event->subtype = subtype;
    event->payload_size = payload_size;
    
    if (payload && payload_size > 0) {
        event->payload = payload;
    } else {
        event->payload = NULL;
    }
    
    return event;
}

void free_event(event_t* event) {
    if (event) {
        if (event->payload) {
            free(event->payload);
        }
        free(event);
    }
}

void print_event(const event_t* event) {
    if (!event) return;
    printf("Event Type: %d, Subtype: %d, Payload Size: %d\n", event->type, event->subtype, event->payload_size);
    if (event->payload && event->payload_size > 0) {
        printf("Payload: ");
        for (uint16_t i = 0; i < event->payload_size; i++) {
            printf("%02x ", ((uint8_t*)event->payload)[i]);
        }
        printf("\n");
    }
}

bool is_event_type(const event_t* event, event_type_t type) {
    return event && event->type == type;
}

bool is_event_subtype(const event_t* event, event_subtype_t subtype) {
    return event && event->subtype == subtype;
}

bool is_event_payload_empty(const event_t* event) {
    return event && event->payload_size == 0;
}

bool is_event_valid(const event_t* event) {
    return event && event->type != 0 && event->subtype != 0;
}
bool is_event_payload_valid(const event_t* event) {
    return event && event->payload && event->payload_size > 0;
}


