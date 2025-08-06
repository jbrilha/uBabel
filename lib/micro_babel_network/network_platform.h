#ifndef NETWORK_PLATFORM_H
#define NETWORK_PLATFORM_H

#include "network_config.h"
#include "platform.h"
#include <stdbool.h>

#ifdef BUILD_PICO
#include "pico/cyw43_arch.h"
typedef int (*scan_callback_t)(void *env, const cyw43_ev_scan_result_t *result);

#elif defined(BUILD_ESP32)
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"

typedef void (*scan_callback_t)(void);
#endif

int network_platform_init(void);
void network_platform_enable_sta_mode(void);
void network_platform_enable_ap_mode(const char *ssid, const char *password);
int network_platform_connect_wifi(const char *ssid, const char *password,
                                  int timeout_ms);
void network_platform_disconnect_wifi(void);
int network_platform_wifi_scan(scan_callback_t callback);
bool network_platform_scan_active(void);
void network_platform_poll(void);
int network_platform_link_status(void);

#endif // NETWORK_PLATFORM_H
