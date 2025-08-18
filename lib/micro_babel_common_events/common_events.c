#include "common_events.h"
#include <stdlib.h>

static event_t* create_neighbor_notification(uint8_t* id, uint16_t notification_identifier) {
    event_t* event = NULL;
    uint8_t* id_copy = (uint8_t*) malloc(UUID_SIZE);
    if(id_copy != NULL) {
        memcpy(id_copy,id,UUID_SIZE);
        event = create_event(EVENT_TYPE_NOTIFICATION, notification_identifier, id, UUID_SIZE);
    
        if(event == NULL) {
            free(id_copy);
        }
    }

    return event;
}

event_t* create_neighbor_up_notification(uint8_t* id) {
    return create_neighbor_notification(id, NOTIFICATION_NEIGHBOR_UP);
}

event_t* create_neighbor_down_notification(uint8_t* id) {
    return create_neighbor_notification(id, NOTIFICATION_NEIGHBOR_DOWN);
}