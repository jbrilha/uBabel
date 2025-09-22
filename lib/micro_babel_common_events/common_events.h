#pragma once 

#include "event.h"
#include "comm_manager.h"

#define NOTIFICATION_NEIGHBOR_UP 501
#define NOTIFICATION_NEIGHBOR_DOWN 502
#define REQUEST_REFRESH_MENU 503

#ifdef __cplusplus
extern "C" {
#endif

event_t* create_neighbor_up_notification(uint8_t* id);
event_t* create_neighbor_down_notification(uint8_t* id);

#ifdef __cplusplus
}
#endif
