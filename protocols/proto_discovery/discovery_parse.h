// discovery_parse.h
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "lwip/ip4_addr.h"

#define MAX_DISCOVERY_IPS 8

typedef struct {
    uint8_t  sig[4];             // "mbma"
    uint8_t  uuid[16]; 
    uint16_t addr_count;         // number of IPv4 addresses parsed into addrs[]
    ip4_addr_t addrs[MAX_DISCOVERY_IPS];
    uint16_t unicast_port;       // host order
    uint64_t announce_period;    // host order, units same as sender
} discovery_msg_t;

bool parse_discovery_message(const uint8_t *buf, size_t len, discovery_msg_t *out);
