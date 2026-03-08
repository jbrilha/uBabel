#include "zigbee.h"

static const char *TAG = "ZIGBEE";

static bool zb_initialized = false;

bool zigbee_platform_init(void) {
    // in case users call this themselves before zigbee_start
    if (zb_initialized) {
        return true;
    }

    esp_zb_platform_config_t config = {
        .radio_config = ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ZB_DEFAULT_HOST_CONFIG(),
    };

    if (esp_zb_platform_config(&config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ZigBee stack");
        return false;
    }

    zb_initialized = true;
    return true;
}
