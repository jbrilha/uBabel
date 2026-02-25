#include "esp_netif_types.h"
#include "esp_wifi_types_generic.h"
#include "network_hal.h"

#define MAX_CONNECTIONS 10

static bool esp32_wifi_initialized = false;
static bool esp32_scan_done = false;
static bool esp32_scan_active = false;
static esp_netif_t *esp_netif_sta = NULL;
static esp_netif_t *esp_netif_ap = NULL;
static scan_callback_t esp32_scan_callback = NULL;

static EventGroupHandle_t wifi_event_group;

static const char *TAG = "NETWORK_HAL";

static bool reconnect = false;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_DISCONNECTED_BIT BIT1
// #define WIFI_DISCONNECTED_BIT BIT2

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    switch (event_id) {
    case WIFI_EVENT_SCAN_DONE:
        esp32_scan_done = true;
        esp32_scan_active = false;
        if (esp32_scan_callback) {
            esp32_scan_callback();
        }
        ESP_LOGI(TAG, "sta scan done");
        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "L2 connected");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        if (reconnect) {
            ESP_LOGI(TAG, "sta disconnect, reconnect...");
            esp_wifi_connect();
        } else {
            ESP_LOGI(TAG, "sta disconnect");
        }
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupSetBits(wifi_event_group, WIFI_DISCONNECTED_BIT);
        break;
    default:
        break;
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data) {
    switch (event_id) {
    case IP_EVENT_STA_GOT_IP:
        xEventGroupClearBits(wifi_event_group, WIFI_DISCONNECTED_BIT);
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "DHCP gave us an IP");
        break;
    default:
        break;
    }
}

int network_hal_init(void) {
    if (esp32_wifi_initialized) {
        return true;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create event group once
    wifi_event_group = xEventGroupCreate();

    // Register handler for WIFI events (including scan + STA disconnect)
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    // Register handler for IP events (STA got IP)
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &ip_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t result = esp_wifi_init(&cfg);

    esp32_wifi_initialized = (result == ESP_OK);
    return (result == ESP_OK) ? 0 : -1;
}

void network_hal_enable_sta_mode(void) {
    // create the netif only when we actually want STA mode
    if (!esp_netif_sta) {
        esp_netif_sta = esp_netif_create_default_wifi_sta();
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void network_hal_enable_ap_mode(const char *ssid, const char *password) {
    bool open = !(password && strlen(password));
    int auth = open ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    // create the netif only when we actually want AP mode
    if (!esp_netif_ap) {
        esp_netif_ap = esp_netif_create_default_wifi_ap();
    }

    wifi_config_t wifi_config = {
        .ap =
            {
                .ssid_len = strlen(ssid),
                .authmode = auth,
                .max_connection = MAX_CONNECTIONS,
            },
    };

    strncpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid) - 1);
    if (!open) {
        strncpy((char *)wifi_config.ap.password, password,
                sizeof(wifi_config.ap.password) - 1);
    }

    wifi_interface_t wifi_interface;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
    wifi_interface = ESP_IF_WIFI_AP;
#else
    wifi_interface = WIFI_IF_AP;
#endif

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(wifi_interface, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

bool network_hal_connect_wifi(const char *ssid, const char *password,
                              int timeout_ms) {
    bool open = !(password && strlen(password));
    int auth = open ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    int bits =
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, 0, 1, 0);

    wifi_config_t wifi_config = {0};

    // SSID & password
    strncpy((char *)wifi_config.sta.ssid, ssid,
            sizeof(wifi_config.sta.ssid) - 1);
    if (!open) {
        strncpy((char *)wifi_config.sta.password, password,
                sizeof(wifi_config.sta.password) - 1);
    } else {
        wifi_config.sta.password[0] = '\0';
    }

    // Accept open or at least WPA2
    wifi_config.sta.threshold.authmode = auth;

    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    // Ensure STA netif exists and WiFi is in STA mode + started
    // network_hal_enable_sta_mode();

    // reset connection if connecting to new AP
    if (bits & WIFI_CONNECTED_BIT) {
        reconnect = false;
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_ERROR_CHECK(esp_wifi_disconnect());
        xEventGroupWaitBits(wifi_event_group, WIFI_DISCONNECTED_BIT, 0, 1,
                            portTICK_PERIOD_MS);
    }

    reconnect = true;
    esp_wifi_disconnect();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    esp_err_t result = esp_wifi_connect();
    if (result != ESP_OK) {
        return false;
    }

    // Wait for CONNECTED or DISCONNECTED or timeout
    bits = xEventGroupWaitBits(
        wifi_event_group, WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT,
        pdTRUE,  // clear bits on exit
        pdFALSE, // wait for either bit
        pdMS_TO_TICKS(timeout_ms > 0 ? timeout_ms : 5000));

    if (bits & WIFI_CONNECTED_BIT) {
        return true; // success: got IP
    } else {
        // either FAIL bit set or timeout
        esp_wifi_disconnect();
        return false;
    }
}

void network_hal_disconnect_wifi(void) {
    reconnect = false;
    esp_wifi_disconnect();
}

int network_hal_wifi_scan(scan_callback_t callback) {
    esp32_scan_callback = callback;
    esp32_scan_active = true;
    esp32_scan_done = false;
    esp_err_t result = esp_wifi_scan_start(NULL, false);
    return (result == ESP_OK) ? 0 : -1;
}

bool network_hal_scan_active(void) { return !esp32_scan_done; }

void network_hal_poll(void) {
    // event-driven so we don't (can't?) poll like that
    vTaskDelay(1);
}

int network_hal_link_status(void) {
    wifi_ap_record_t ap_info;
    esp_err_t result = esp_wifi_sta_get_ap_info(&ap_info);
    if (result == ESP_OK) {
        return 1; // equivalent to CYW43_LINK_JOIN
    } else {
        return 0;
    }
}

uint32_t network_hal_get_local_ip(void) {
    int bits =
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, 0, 1, 0);
    esp_netif_t *ifx = esp_netif_sta;
    esp_netif_ip_info_t ip_info;
    wifi_mode_t mode;

    esp_wifi_get_mode(&mode);
    if (WIFI_MODE_STA == mode) {
        bits =
            xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, 0, 1, 0);
        if (bits & WIFI_CONNECTED_BIT) {
            ifx = esp_netif_sta;
        } else {
            ESP_LOGE(TAG, "sta has no IP");
            return 0;
        }
    }

    esp_netif_get_ip_info(ifx, &ip_info);
    return ip_info.ip.addr;
}
