#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "mem_check.h"
#include "nvs_flash.h"

#include "event_dispatcher.h"
#include "network_manager.h"
#include "platform.h"

#include "lora.h"
#include "sx126x.h"
#include "i2c_hal.h"
#include "sd_card.h"
#include "spi_hal.h"
#include "ui_manager.h"

#include "comm_manager.h"
#include "common_events.h"
#include "event_dispatcher.h"
#include "network_events.h"
#include "network_manager.h"
#include "proto_iot_control.h"
#include "proto_simple_overlay.h"

#include "bmp280.h"
#include "camera.h"
#include "dht11.h"
#include "lcd16x2.h"
#include "mma7660.h"
#include "paj7620.h"
#include "tca9548.h"

static const char *TAG = "ESP32_MAIN";

void init_peripherals(void) {
    spi_hal_master_init();
    
    lora_radio_t *r = lora_create_sx126x_radio();
    if (lora_init(r)) {
        ESP_LOGI(TAG, "SUCCESS");
    } else {
        ESP_LOGE(TAG, "FUCK");
    }

    // ui_manager_init();

    // mount_sdmmc_card_1w(SDMMC_CLK_PIN, SDMMC_CMD_PIN, SDMMC_D0_PIN);
    // ui_manager_set_tardis_widget();

    // i2c_init_default();

    // TCA9548_init();
    // TCA9548_open_all_channels();
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());

    xTaskCreate(mem_check_task,   // Task function
                "mem_check_task", // Task name
                4096,             // Stack size in words
                NULL,             // Task parameters
                1,                // Priority
                NULL              // Task handle
    );

    // event_dispatcher_init();
    // comm_manager_init();
    init_peripherals();

    // this provides a live feed @ ~25 FPS
    // ui_manager_set_camera_widget();
    // run_camera_task();
    // run_sd_card_task();

    // xTaskCreate(network_manager_task, // Task function
    //             "NetworkManager",     // Task name
    //             4096,                 // Stack size in words
    //             NULL,                 // Task parameters
    //             5,                    // Priority
    //             NULL                  // Task handle
    // );
    //
    // simple_overlay_network_init();
    // iot_control_protocol_init();

    vTaskDelete(NULL);
}
