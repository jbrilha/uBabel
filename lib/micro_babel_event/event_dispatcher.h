#ifndef MICRO_BABEL_EVENT_DISPATCHER_H
#define MICRO_BABEL_EVENT_DISPATCHER_H

#include "platform.h"
#include "event.h"
#include "proto_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

bool event_dispatcher_init(void);

// Register a task queue for a specific event type+subtype
bool event_dispatcher_register(QueueHandle_t queue, event_type_t type, uint16_t subtype);
// Unegister a task queue for a specific event type+subtype
bool event_dispatcher_unregister(QueueHandle_t queue, event_type_t type, uint16_t subtype);

// Post an event to be dispatched
bool event_dispatcher_post(event_t* event);

#ifdef __cplusplus
}
#endif

#endif // MICRO_BABEL_EVENT_DISPATCHER_H
