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

#include "esp_lora.h"
#include "spi_manager.h"
#include "ui_manager.h"

#include "comm_manager.h"
#include "common_events.h"
#include "event_dispatcher.h"
#include "network_events.h"
#include "network_manager.h"
#include "proto_iot_control.h"
#include "proto_simple_overlay.h"

static const char *TAG = "M5_MAIN";

typedef enum { node, device, action, parameter } navigation_mode_t;

static QueueHandle_t application_queue;
static iot_node_handle_t node_handle;
static iot_device_handle_t device_handle;
static navigation_mode_t nav;
static const device_t* device_info;

void application_task(void *pvParameters) {
    char output[256];
    event_t *event = NULL;

    while (true) {
        memset(output, 0, 256); // Clear text buffer
        if (xQueueReceive(application_queue, &event, portMAX_DELAY) == pdTRUE) {
            printf("Application main loop has received event: type=%d "
                   "subtype=%d\n",
                   event->type, event->subtype);

            free_event(event);
            event = NULL;
        }
    }
}

void application_init() {
    application_queue = xQueueCreate(10, sizeof(event_t *));

    nav = node;
    device_info = get_device_info_data();

    node_handle = initialize_node_iterator();

    xTaskCreate(application_task, "main_app_task", configMINIMAL_STACK_SIZE,
                NULL, 3, NULL);
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

    spi_manager_init();

    ui_manager_init();

#if M5STACK_RECEIVER
    ui_manager_set_lora_rec_widget();
    esp_lora_start_receiver();
#elif M5STACK_SENDER
    ui_manager_set_lora_sndr_widget();
    esp_lora_start_sender();
#endif

    xTaskCreate(network_manager_task, // Task function
                "NetworkManager",     // Task name
                4096,                 // Stack size in words
                NULL,                 // Task parameters
                5,                    // Priority
                NULL                  // Task handle
    );

    simple_overlay_network_init();
    iot_control_protocol_init();

    application_init();
}
