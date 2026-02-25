#ifndef NETWORK_HAL_H
#define NETWORK_HAL_H

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

int network_hal_init(void);
void network_hal_enable_sta_mode(void);
void network_hal_enable_ap_mode(const char *ssid, const char *password);
bool network_hal_connect_wifi(const char *ssid, const char *password,
                                  int timeout_ms);
void network_hal_disconnect_wifi(void);
int network_hal_wifi_scan(scan_callback_t callback);
bool network_hal_scan_active(void);
void network_hal_poll(void);
int network_hal_link_status(void);

uint32_t network_hal_get_local_ip(void);

#endif // NETWORK_HAL_H
