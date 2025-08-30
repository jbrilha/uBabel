#ifndef LORA_TYPES_H
#define LORA_TYPES_H

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t recipient_id;
    uint8_t sender_id;
    uint16_t message_id;
    uint8_t payload_len;
    uint8_t payload[];
} lora_pkt_t;

typedef struct {
    int16_t rssi;
    float snr;
    int32_t freq_err;
    lora_pkt_t *pkt;
    uint16_t pkt_size;
} lora_info_t;

#endif // !LORA_TYPES_H
