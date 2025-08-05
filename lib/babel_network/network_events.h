// network_events.h
#pragma once

#include "lwip/ip_addr.h"

typedef enum {
    EVENT_WIFI_CONNECTED,
    EVENT_WIFI_DISCONNECTED,
    EVENT_ETH_CONNECTED,
    EVENT_ETH_DISCONNECTED
} network_event_type_t;

typedef struct {
    network_event_type_t type;
    ip_addr_t ip;
    char ssid[33]; // null-terminated, max 32 chars + null
} network_event_t;