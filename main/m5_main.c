#include <stdint.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "mem_check.h"
#include "nvs_flash.h"

#include "event_dispatcher.h"
#include "network_manager.h"

#include "m5_lora.h"
// #include "m5_display.h"
#include "spi_lcd_touch.h"

static const char *TAG = "M5_MAIN";

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

    // LORA INIT MUST COME BEFORE LCD INIT due to SPI shenanigans
    start_lora_sender_task();
    // start_lora_receiver_task();

    xTaskCreate(lcd_init_task,   // Task function
                "lcd_init_task", // Task name
                4096,            // Stack size in words
                NULL,            // Task parameters
                1,               // Priority
                NULL             // Task handle
    );

}
