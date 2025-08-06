// network_events.h
#pragma once

#include "lwip/ip_addr.h"


#define EVENT_WIFI_CONNECTED 101
#define EVENT_WIFI_DISCONNECTED 102


typedef struct {
    uint16_t type;;
    char ssid[33]; // null-terminated, max 32 chars + null
    char ip[16]; // null-terminated, max 15 chars + null
} network_event_t;