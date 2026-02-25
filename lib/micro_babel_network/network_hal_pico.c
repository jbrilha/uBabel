#include "network_hal.h"

#define MAX_CONNECTIONS 10

int network_hal_init(void) { return cyw43_arch_init(); }

void network_hal_enable_sta_mode(void) { cyw43_arch_enable_sta_mode(); }

void network_hal_enable_ap_mode(const char *ssid, const char *password) {
    bool open = !(password && strlen(password));
    int auth = open ? CYW43_AUTH_OPEN : CYW43_AUTH_WPA2_AES_PSK;
    cyw43_arch_enable_ap_mode(ssid, password, auth);
}

bool network_hal_connect_wifi(const char *ssid, const char *password,
                              int timeout_ms) {
    bool open = !(password && strlen(password));
    int auth = open ? CYW43_AUTH_OPEN : CYW43_AUTH_WPA2_AES_PSK;
    return cyw43_arch_wifi_connect_timeout_ms(ssid, password, auth,
                                              timeout_ms) == 0;
}

extern cyw43_t cyw43_state;

void network_hal_disconnect_wifi(void) {
    cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
}

int network_hal_wifi_scan(scan_callback_t callback) {
    cyw43_wifi_scan_options_t scan_opts = {0};
    return cyw43_wifi_scan(&cyw43_state, &scan_opts, NULL, callback);
    // todo callback
}

bool network_hal_scan_active(void) {
    return cyw43_wifi_scan_active(&cyw43_state);
}

void network_hal_poll(void) { cyw43_arch_poll(); }

int network_hal_link_status(void) {
    return cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
}

uint32_t network_hal_get_local_ip(void) {
    uint8_t *lwip_address = (uint8_t *)&(netif_default->ip_addr.addr);
    printf("LWIP address %d.%d.%d.%d\n", lwip_address[0], lwip_address[1],
           lwip_address[2], lwip_address[3]);

    uint32_t ip32 = (lwip_address[0] << 24 | 
                     lwip_address[1] << 16 |
                     lwip_address[2] << 8 |
                     lwip_address[3]);

    return ip32;
}
