#include "event.h"

static const char *TAG = "EVENT";

static SemaphoreHandle_t event_mutex;

void init_event_mutex() {
    event_mutex = xSemaphoreCreateMutex();
}

event_t* create_event_with_destination(event_type_t type, event_subtype_t subtype, void* payload, uint16_t payload_size, uint16_t protocol_id) {
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

    event->reference_counter = 0;
    event->proto_source = protocol_id;
    event->proto_destination = MICRO_BABEL_SYSTEM_PROTOCOL;
    
    return event;
}

event_t* create_event(event_type_t type, event_subtype_t subtype, void* payload, uint16_t payload_size) {
    return create_event_with_destination(type, subtype, payload, payload_size, MICRO_BABEL_SYSTEM_PROTOCOL);
}

void free_event(event_t* event) {
    xSemaphoreTake(event_mutex, portMAX_DELAY);
    
    if (event) {
        event->reference_counter--;
        if(event->reference_counter <= 0) {
            if(event->payload != NULL) {
             if(event->type == EVENT_TYPE_MESSAGE && EVENT_MESSAGE_SEND) {
                free_message(event->payload);
             } else {     
                free(event->payload);
             }
            }    
            
            free(event);
        }
    }

    xSemaphoreGive(event_mutex);
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

/****** Dealing with Messages ******/

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