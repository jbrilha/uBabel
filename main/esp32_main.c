#include <stdint.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "nvs_flash.h"
#include "tcp.h"
#include "udp.h"

#define MAX_MSG_SIZE 128
#define MAX_PEERS 5

#define WIFI_SSID "ESP32_AP"
#define WIFI_PASS "superSafeAP"

#define UDP_SERVER_TASK_STACK_SIZE 4096
#define UDP_SERVER_TASK_PRIORITY 5

#define TCP_SERVER_TASK_STACK_SIZE 4096
#define TCP_SERVER_TASK_PRIORITY 5

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "AP started");
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "station connected to AP");
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(TAG, "station disconnected from AP");
    }
}

void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                               &wifi_event_handler, NULL);

    wifi_config_t wifi_config = {
        .ap = {.ssid = WIFI_SSID,
               .password = WIFI_PASS,
               .ssid_len = strlen(WIFI_SSID),
               .authmode = WIFI_AUTH_WPA_WPA2_PSK,
               .max_connection = MAX_PEERS},
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init();

    vTaskDelay(pdMS_TO_TICKS(3000));

    xTaskCreate(udp_server_task, "udp_server", UDP_SERVER_TASK_STACK_SIZE, NULL,
                UDP_SERVER_TASK_PRIORITY, NULL);
    xTaskCreate(tcp_server_task, "tcp_server", TCP_SERVER_TASK_STACK_SIZE, NULL,
                TCP_SERVER_TASK_PRIORITY, NULL);
}
