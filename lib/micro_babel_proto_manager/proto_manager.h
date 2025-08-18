//proto_manager.h
#pragma once

#include <stdio.h>
#include "platform.h"

#ifdef __cplusplus
extern "C" {
#endif

// Register a protocol (i.e. special tasks) queue associated to its protocols id.
bool proto_manager_register_protocol(QueueHandle_t queue, uint16_t protocol_id);

// Return the queue of a previously registered protocol
QueueHandle_t find_protocol(uint16_t proto_id);

#ifdef __cplusplus
}
#endif
