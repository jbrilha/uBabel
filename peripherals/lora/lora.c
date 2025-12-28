#include "lora.h"

static const char *TAG = "LoRa Driver";

bool lora_init_w_config(lora_radio_t *r, const lora_config_t *config) {
    if (!r || !r->init) {
        return false;
    }

    return r->init(r, config);
}

bool lora_init(lora_radio_t *r) {
    if (!r || !r->init) {
        return false;
    }

    lora_config_t config = {
        .frequency = 868000000,
        .offset = 0,
        .tx_power = 10,
    };

    return r->init(r, &config);
}

int lora_transmit(lora_radio_t *r, const uint8_t *data, size_t len,
                  uint32_t timeout_ms) {
    if (!r || !r->transmit) {
        return 0;
    }

    return r->transmit(r, data, len, timeout_ms);
}
