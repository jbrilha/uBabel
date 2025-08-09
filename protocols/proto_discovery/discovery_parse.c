// discovery_parse.c
#include "discovery_parse.h"
#include <string.h>

static bool read_be16(const uint8_t **p, size_t *rem, uint16_t *out) {
    if (*rem < 2) return false;
    const uint8_t *b = *p;
    *out = (uint16_t)((b[0] << 8) | b[1]);
    *p += 2; *rem -= 2;
    return true;
}

static bool read_be64(const uint8_t **p, size_t *rem, uint64_t *out) {
    if (*rem < 8) return false;
    const uint8_t *b = *p;
    *out = ((uint64_t)b[0] << 56) | ((uint64_t)b[1] << 48) |
           ((uint64_t)b[2] << 40) | ((uint64_t)b[3] << 32) |
           ((uint64_t)b[4] << 24) | ((uint64_t)b[5] << 16) |
           ((uint64_t)b[6] << 8)  |  (uint64_t)b[7];
    *p += 8; *rem -= 8;
    return true;
}

bool parse_discovery_message(const uint8_t *buf, size_t len, discovery_msg_t *out) {
    if (!buf || !out || len < 4 + 8 + 8 + 2) return false;

    memset(out, 0, sizeof(*out));

    // signature
    if (buf[0] != 'm' || buf[1] != 'b' || buf[2] != 'm' || buf[3] != 'a') return false;
    memcpy(out->sig, buf, 4);

    const uint8_t *p = buf + 4;
    size_t rem = len - 4;

    if (rem < 16) return false;
    memcpy(out->uuid, p, 16);
    p += 16; rem -= 16;

    uint16_t naddr = 0;
    if (!read_be16(&p, &rem, &naddr)) return false;

    // cap to our array size; still advance through all bytes
    uint16_t to_store = naddr > MAX_DISCOVERY_IPS ? MAX_DISCOVERY_IPS : naddr;

    // need 4*naddr bytes for addresses
    if (rem < (size_t)4 * naddr) return false;

    for (uint16_t i = 0; i < naddr; ++i) {
        ip4_addr_t ip;
        // p[0..3] are already in network order; IP4_ADDR expects bytes in host order values
        IP4_ADDR(&ip, p[0], p[1], p[2], p[3]);
        if (i < to_store) out->addrs[i] = ip;
        p += 4; rem -= 4;
    }
    out->addr_count = to_store;

    if (!read_be16(&p, &rem, &out->unicast_port)) return false;

    if (!read_be64(&p, &rem, &out->announce_period)) return false;

    // Optional: verify we consumed exactly the buffer (or allow trailing bytes)
    // if (rem != 0) return false;

    return true;
}
