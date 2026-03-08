#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "mem_check.h"
#include "nvs_flash.h"

#include "esp32c6_buttons.h"
#include "event_dispatcher.h"
#include "network_manager.h"
#include "platform.h"

#include "i2c_hal.h"
#include "iperf_hal.h"
#include "lora.h"
#include "sd_card.h"
#include "spi_hal.h"
#include "sx126x.h"
#include "ui_manager.h"

#include "comm_manager.h"
#include "common_events.h"
#include "event_dispatcher.h"
#include "network_events.h"
#include "network_manager.h"
#include "proto_iot_control.h"
#include "proto_simple_overlay.h"

#include "bt_host.h"
#include "zigbee.h"

static const char *TAG = "ESP32_MAIN";

static lora_radio_t *r = NULL;

bool init_lora(void) {
    r = lora_create_sx126x_radio();
    if (r && lora_init(r)) {
        ESP_LOGW(TAG, "SUCCESS");
        return true;
    }
    ESP_LOGE(TAG, "FUCK");
    return false;
}

void init_peripherals(void) {
    i2c_init_default();
    spi_hal_master_init();

    // ui_manager_init();

    if (init_lora()) {
        lora_start_sender(r);
        ui_manager_set_lora_sndr_widget();
        // lora_start_receiver(r);
        // ui_manager_set_lora_rec_widget();
    }
#if CONFIG_IDF_TARGET_ESP32C6
    esp32c6_buttons_init();
    run_esp32c6_buttons_task();

    ui_manager_set_tardis_widget();
#endif
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());

    event_dispatcher_init();
    comm_manager_init();
    init_peripherals();

    xTaskCreate(network_manager_task, "NetworkManager", 4096, NULL, 5, NULL);

    simple_overlay_network_init();
    iot_control_protocol_init();

    vTaskDelay(500);
    bt_host_init_as_peripheral();

    vTaskDelay(500);
    zigbee_start();

    vTaskDelay(1000);
    run_mem_check();

    vTaskDelete(NULL);
}
