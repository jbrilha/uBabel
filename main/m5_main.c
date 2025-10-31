#include <stdint.h>
#include <string.h>

#include "m5_buttons.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "mem_check.h"
#include "nvs_flash.h"

#include "event_dispatcher.h"
#include "network_manager.h"

#include "app.h"
#include "esp_lora.h"
#include "i2c_hal.h"
#include "platform.h"
#include "spi_manager.h"
#include "ui_manager.h"

#include "comm_manager.h"
#include "common_events.h"
#include "event_dispatcher.h"
#include "network_events.h"
#include "network_manager.h"
#include "proto_iot_control.h"
#include "proto_simple_overlay.h"

#include "bmp280.h"
#include "dht11.h"
#include "lcd16x2.h"
#include "mma7660.h"
#include "paj7620.h"
#include "tca9548.h"

static const char *TAG = "M5_MAIN";

void init_peripherals(void) {
    spi_manager_init();
    ui_manager_init();

    m5_buttons_init();

#if M5STACK_RECEIVER
    esp_lora_init();

    ui_manager_set_lora_rec_widget();
    esp_lora_start_receiver();
#elif M5STACK_SENDER
    esp_lora_init();

    ui_manager_set_lora_sndr_widget();
    esp_lora_start_sender();
#else
    ui_manager_set_tardis_widget();
#endif

    run_m5_buttons_task();

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

    event_dispatcher_init();
    comm_manager_init();
    init_peripherals();

    xTaskCreate(network_manager_task, // Task function
                "NetworkManager",     // Task name
                4096,                 // Stack size in words
                NULL,                 // Task parameters
                5,                    // Priority
                NULL                  // Task handle
    );

    simple_overlay_network_init();
    iot_control_protocol_init();

    // application_init(); // the UI handles all the logic and interactions with the iot proto

    vTaskDelete(NULL);
}
