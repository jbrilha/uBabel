#include "network_hal.h"

#define MAX_CONNECTIONS 10

#ifdef BUILD_ESP32
static bool esp32_wifi_initialized = false;
static bool esp32_scan_done = false;
static bool esp32_scan_active = false;
static esp_netif_t *esp_netif_sta = NULL;
static esp_netif_t *esp_netif_ap = NULL;
static scan_callback_t esp32_scan_callback = NULL;

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_SCAN_DONE) {
            esp32_scan_done = true;
            esp32_scan_active = false;
            if (esp32_scan_callback) {
                esp32_scan_callback();
            }
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            // we got disconnected or failed to connect
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        // You can also log WIFI_EVENT_STA_CONNECTED here if you want.
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            // DHCP gave us an IP
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}
#endif

int network_hal_init(void) {
#ifdef BUILD_PICO
    return cyw43_arch_init();
#elif defined(BUILD_ESP32)
    if (esp32_wifi_initialized) {
        return true;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create event group once
    s_wifi_event_group = xEventGroupCreate();

    // Register handler for WIFI events (including scan + STA disconnect)
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    // Register handler for IP events (STA got IP)
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t result = esp_wifi_init(&cfg);

    esp32_wifi_initialized = (result == ESP_OK);
    return (result == ESP_OK) ? 0 : -1;
#endif
}

void network_hal_enable_sta_mode(void) {
#ifdef BUILD_PICO
    cyw43_arch_enable_sta_mode();
#elif defined(BUILD_ESP32)
    // create the netif only when we actually want STA mode
    if (!esp_netif_sta) {
        esp_netif_sta = esp_netif_create_default_wifi_sta();
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
#endif
}

void network_hal_enable_ap_mode(const char *ssid, const char *password) {
    bool open = !(password && strlen(password));
    int auth;
#ifdef BUILD_PICO
    auth = open ? CYW43_AUTH_OPEN : CYW43_AUTH_WPA2_AES_PSK;
    cyw43_arch_enable_ap_mode(ssid, password, auth);
#elif defined(BUILD_ESP32)
    // create the netif only when we actually want AP mode
    if (!esp_netif_ap) {
        esp_netif_ap = esp_netif_create_default_wifi_ap();
    }

    auth = open ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

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
    ESP_ERROR_CHECK(
        esp_wifi_set_config(wifi_interface, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
#endif
}

int network_hal_connect_wifi(const char *ssid, const char *password,
                                  int timeout_ms) {
    int auth;
    bool open = !(password && strlen(password));
#ifdef BUILD_PICO
    auth = open ? CYW43_AUTH_OPEN : CYW43_AUTH_WPA2_AES_PSK;
    return cyw43_arch_wifi_connect_timeout_ms(ssid, password, auth, timeout_ms);
#elif defined(BUILD_ESP32)
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
    wifi_config.sta.threshold.authmode =
        open ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    // Ensure STA netif exists and WiFi is in STA mode + started
    network_hal_enable_sta_mode();

    // Clear previous bits from any past attempt
    xEventGroupClearBits(s_wifi_event_group,
                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    // Apply config
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // Make sure we’re disconnected before connecting
    esp_wifi_disconnect();

    esp_err_t result = esp_wifi_connect();
    if (result != ESP_OK) {
        return -1;
    }

    // Wait for CONNECTED or FAIL or timeout
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdTRUE,  // clear bits on exit
        pdFALSE, // wait for either bit
        pdMS_TO_TICKS(timeout_ms > 0 ? timeout_ms : 10000));

    if (bits & WIFI_CONNECTED_BIT) {
        return 0; // success: got IP
    } else {
        // either FAIL bit set or timeout
        esp_wifi_disconnect();
        return -1;
    }
#endif
}

#ifdef BUILD_PICO
extern cyw43_t cyw43_state;
#endif

void network_hal_disconnect_wifi(void) {
#ifdef BUILD_PICO
    cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    // cyw43_arch_wifi_connect_async(NULL, NULL, 0); // aka disconnect
#elif defined(BUILD_ESP32)
    esp_wifi_disconnect();
#endif
}

int network_hal_wifi_scan(scan_callback_t callback) {
#ifdef BUILD_PICO
    cyw43_wifi_scan_options_t scan_opts = {0};
    return cyw43_wifi_scan(&cyw43_state, &scan_opts, NULL, callback);
    // todo callback
#elif defined(BUILD_ESP32)
    esp32_scan_callback = callback;
    esp32_scan_active = true;
    esp32_scan_done = false;
    esp_err_t result = esp_wifi_scan_start(NULL, false);
    return (result == ESP_OK) ? 0 : -1;
#endif
}

bool network_hal_scan_active(void) {
#ifdef BUILD_PICO
    return cyw43_wifi_scan_active(&cyw43_state);
#elif defined(BUILD_ESP32)
    return !esp32_scan_done;
#endif
}

void network_hal_poll(void) {
#ifdef BUILD_PICO
    cyw43_arch_poll();
#elif defined(BUILD_ESP32)
    // event-driven so we don't (can't?) poll like that
    vTaskDelay(pdMS_TO_TICKS(1));
#endif
}

int network_hal_link_status(void) {
#ifdef BUILD_PICO
    return cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
#elif defined(BUILD_ESP32)
    wifi_ap_record_t ap_info;
    esp_err_t result = esp_wifi_sta_get_ap_info(&ap_info);
    if (result == ESP_OK) {
        return 1; // equivalent to CYW43_LINK_JOIN
    } else {
        return 0;
    }
#endif
}
