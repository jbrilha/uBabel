#include <stdint.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "mem_check.h"
#include "nvs_flash.h"
#include "spi_lcd_touch.h"
#include "tcp.h"
#include "udp.h"

#include "event_dispatcher.h"
#include "network_manager.h"

static const char *TAG = "ESP32_MAIN";

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

    // wifi_init();
    //
    // vTaskDelay(pdMS_TO_TICKS(3000));
    //
    // xTaskCreate(udp_server_task, "udp_server", UDP_SERVER_TASK_STACK_SIZE,
    // NULL,
    //             UDP_SERVER_TASK_PRIORITY, NULL);
    // xTaskCreate(tcp_server_task, "tcp_server", TCP_SERVER_TASK_STACK_SIZE,
    // NULL,
    //             TCP_SERVER_TASK_PRIORITY, NULL);

    // xTaskCreate(
    //     network_manager_task,   // Task function
    //     "NetworkManager",       // Task name
    //     4096,                   // Stack size in words
    //     NULL,                   // Task parameters
    //     1,                      // Priority
    //     NULL                    // Task handle
    // );

    xTaskCreate(lcd_init_task,   // Task function
                "lcd_init_task", // Task name
                4096,            // Stack size in words
                NULL,            // Task parameters
                1,               // Priority
                NULL             // Task handle
    );
}
