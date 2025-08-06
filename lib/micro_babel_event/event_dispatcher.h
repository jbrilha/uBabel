#ifndef MICRO_BABEL_EVENT_DISPATCHER_H
#define MICRO_BABEL_EVENT_DISPATCHER_H

#include "event.h"
#include "FreeRTOS.h"
#include "queue.h"


#define QUEUE_INITIAL_CAPACITY 3

typedef struct event_subtype_subscription {
    event_subtype_t subtype;
    uint16_t queue_count; // Number of queues subscribed to this subtype
    uint16_t queue_capacity; // Capacity of the queues array
    QueueHandle_t* queues;
    struct event_subtype_subscription* next; // Pointer to the next subscription in the linked list
} event_subtype_subscription_t;

typedef struct event_type_subscription {
    event_type_t type;
    event_subtype_subscription_t* subtypes; // Linked list of subtypes
} event_type_subscription_t;



#ifdef __cplusplus
extern "C" {
#endif

bool event_dispatcher_init(void);

// Register a task queue for a specific event type+subtype
bool event_dispatcher_register(QueueHandle_t queue, event_type_t type, uint16_t subtype);

// Post an event to be dispatched
bool event_dispatcher_post(event_t* event);

#ifdef __cplusplus
}
#endif

#endif // MICRO_BABEL_EVENT_DISPATCHER_H

