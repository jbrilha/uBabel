// network_manager.c
#include "network_manager.h"
#include "event_dispatcher.h"
#include "network_config.h"
#include "network_events.h"

#include "network_platform.h"

#include "lwip/dns.h"
#include "lwip/icmp.h"
#include "lwip/inet.h"
#include "lwip/inet_chksum.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/prot/ip4.h"
#include "lwip/raw.h"
#include "lwip/sockets.h"
#include "lwip/timeouts.h"
#include "lwip/udp.h"

typedef struct open_network_info {
    char ssid[MAX_SSID_LEN + 1];
    int rssi;
    struct open_network_info *next;
} open_network_info_t;

open_network_info_t *open_networks = NULL;

bool connected = false;
bool scan_in_progress = false;
bool scan_has_completed = false;

void add_open_network(const char *ssid, int rssi) {
    open_network_info_t *existing = open_networks;
    while (existing) {
        if (strcmp(existing->ssid, ssid) == 0) {
            // Network already exists, update RSSI if higher
            if (rssi > existing->rssi) {
                existing->rssi = rssi;
            }
            return; // No need to add again
        }
        existing = existing->next;
    }

    open_network_info_t *new_network = malloc(sizeof(open_network_info_t));
    if (!new_network) {
        printf("Failed to allocate memory for open network info\n");
        return;
    }
    strncpy(new_network->ssid, ssid, MAX_SSID_LEN);
    new_network->ssid[MAX_SSID_LEN] = '\0'; // ensure null-termination
    new_network->rssi = rssi;
    new_network->next = NULL;

    open_network_info_t **current = &open_networks;

    bool set = false;
    while (!set) {
        if (*current == NULL) {
            *current = new_network;
            set = true;
        } else if (new_network->rssi > (*current)->rssi) {
            new_network->next = *current;
            *current = new_network;
            set = true;
        } else {
            current = &((*current)->next);
        }
    }
}

void print_open_networks() {
    open_network_info_t *current = open_networks;
    while (current) {
        printf("Open Network: SSID: %s, RSSI: %d\n", current->ssid,
               current->rssi);
        current = current->next;
    }
}

void recursive_free_open_networks(open_network_info_t *network) {
    if (network == NULL)
        return;
    recursive_free_open_networks(network->next);
    free(network);
}

void free_open_networks() {
    recursive_free_open_networks(open_networks);
    open_networks = NULL;
}

static bool try_connect_wifi(const wifi_credentials_t *creds) {
    printf("Trying SSID: %s (%s)\n", creds->ssid,
           strlen(creds->pass) ? "secured" : "open");

    return network_platform_connect_wifi(creds->ssid, creds->pass, 10000) == 0;
}

/* static int scan_result(void *env, const cyw43_ev_scan_result_t *result) {
    if (result) {
        printf("ssid: %-32s rssi: %4d chan: %3d mac:
%02x:%02x:%02x:%02x:%02x:%02x sec: %u\n", result->ssid, result->rssi,
result->channel, result->bssid[0], result->bssid[1], result->bssid[2],
result->bssid[3], result->bssid[4], result->bssid[5], result->auth_mode);

        if (result->auth_mode == CYW43_AUTH_OPEN && strlen(result->ssid) > 0) {
            printf("Found open network: %s (RSSI: %d)\n", result->ssid,
result->rssi); add_open_network(result->ssid, result->rssi);
        }
    }
    return 0;
} */

#ifdef BUILD_PICO
static int scan_result(void *env, const cyw43_ev_scan_result_t *result) {
    if (result) {
        printf("ssid: %-32s rssi: %4d chan: %3d mac: "
               "%02x:%02x:%02x:%02x:%02x:%02x sec: %u\n",
               result->ssid, result->rssi, result->channel, result->bssid[0],
               result->bssid[1], result->bssid[2], result->bssid[3],
               result->bssid[4], result->bssid[5], result->auth_mode);
        if (result->auth_mode == CYW43_AUTH_OPEN && strlen(result->ssid) > 0) {
            printf("Found open network: %s (RSSI: %d)\n", result->ssid,
                   result->rssi);
            add_open_network(result->ssid, result->rssi);
        }
    }
    return 0;
}
#elif defined(BUILD_ESP32)
static void scan_result(void) {
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);

    if (ap_count > 0) {
        wifi_ap_record_t *ap_records =
            malloc(sizeof(wifi_ap_record_t) * ap_count);
        if (ap_records &&
            esp_wifi_scan_get_ap_records(&ap_count, ap_records) == ESP_OK) {
            for (int i = 0; i < ap_count; i++) {
                printf("ssid: %-32s rssi: %4d chan: %3d sec: %u\n",
                       ap_records[i].ssid, ap_records[i].rssi,
                       ap_records[i].primary, ap_records[i].authmode);

                if (ap_records[i].authmode == WIFI_AUTH_OPEN &&
                    strlen((char *)ap_records[i].ssid) > 0) {
                    printf("Found open network: %s (RSSI: %d)\n",
                           ap_records[i].ssid, ap_records[i].rssi);
                    add_open_network((char *)ap_records[i].ssid,
                                     ap_records[i].rssi);
                }
            }
        }
        free(ap_records);
    }
}
#endif

void scan_for_open_networks() {
    if (scan_in_progress) {
        // Already scanning: keep polling
        for (int i = 0; i < 1000; ++i) { // ~1 second
            network_platform_poll();
            vTaskDelay(pdMS_TO_TICKS(10));
            if (!network_platform_scan_active()) {
                break;
            }
        }

        if (!network_platform_scan_active()) {
            printf("Wi-Fi scan completed.\n");
            scan_in_progress = false;
            scan_has_completed = true;
        } else {
            printf("Scan did not complete in time.\n");
        }
        return;
    }

    if (!connected && !scan_in_progress) {
        int err = network_platform_wifi_scan(scan_result);
        if (err != 0) {
            printf("Failed to start Wi-Fi scan: %d\n", err);
            return;
        }

        printf("Starting Wi-Fi scan for open networks...\n");
        scan_in_progress = true;
        scan_has_completed = false;

        // Start polling immediately
        for (int i = 0; i < 1000; ++i) { // ~1 second
            network_platform_poll();
            vTaskDelay(pdMS_TO_TICKS(10));
            if (!network_platform_scan_active()) {
                break;
            }
        }

        if (!network_platform_scan_active()) {
            printf("Wi-Fi scan completed.\n");
            scan_in_progress = false;
            scan_has_completed = true;
        } else {
            printf("Scan did not complete in time.\n");
        }
    }
}

/**
void scan_for_open_networks() {
    //If the scan is still progressing
    if( scan_in_progress && cyw43_wifi_scan_active(&cyw43_state) &&
!scan_has_completed) { printf("Scan still in progress...\n"); cyw43_arch_poll();
        return;
    }

    if (!scan_in_progress && !connected) {
        cyw43_wifi_scan_options_t scan_opts = {0};
        int err = cyw43_wifi_scan(&cyw43_state, &scan_opts, NULL, &scan_result);
        if (err != 0) {
            printf("Failed to start Wi-Fi scan: %d\n", err);
            return;
        }
        printf("Starting Wi-Fi scan for open networks...\n");
        scan_in_progress = true;
        scan_has_completed = false;
        cyw43_arch_poll();
        return;
    }

    if (scan_in_progress && !cyw43_wifi_scan_active(&cyw43_state)) {
        printf("Wi-Fi scan completed.\n");
        scan_in_progress = false;
        scan_has_completed = true;
    }
}
**/

bool check_connectivity_via_ping_gateway() {
    struct netif *netif = netif_default;
    if (!netif) {
        printf("No default netif\n");
        return false;
    }

    ip_addr_t gw_ip = netif->gw;
    int s = lwip_socket(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
    if (s < 0) {
        printf("Failed to create socket\n");
        return false;
    }

    struct sockaddr_in to;
    to.sin_len = sizeof(to);
    to.sin_family = AF_INET;
#ifdef BUILD_PICO
    to.sin_addr.s_addr = gw_ip.addr;
#elif defined(BUILD_ESP32)
    to.sin_addr.s_addr = ip4_addr_get_u32(ip_2_ip4(&gw_ip));
#endif

    struct icmp_echo_hdr hdr;
    hdr.type = ICMP_ECHO;
    hdr.code = 0;
    hdr.chksum = 0;
    hdr.id = 0xabcd;
    hdr.seqno = htons(1);

    // No payload
    hdr.chksum = inet_chksum(&hdr, sizeof(hdr));

    int sent = lwip_sendto(s, &hdr, sizeof(hdr), 0, (struct sockaddr *)&to,
                           sizeof(to));

    if (sent < 0) {
        printf("Failed to send ping\n");
        lwip_close(s);
        return false;
    }

    struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};
    lwip_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    uint8_t recv_buf[64];
    int recv_len = lwip_recv(s, recv_buf, sizeof(recv_buf), 0);
    lwip_close(s);

    if (recv_len >=
        (int)(sizeof(struct ip_hdr) + sizeof(struct icmp_echo_hdr))) {
        struct ip_hdr *iphdr = (struct ip_hdr *)recv_buf;
        struct icmp_echo_hdr *icmphdr =
            (struct icmp_echo_hdr *)(recv_buf + (IPH_HL(iphdr) * 4));

        if ((icmphdr->id == hdr.id) && (icmphdr->seqno == hdr.seqno)) {
            // printf("Ping to gateway successful\n");
            return true;
        }
    }

    printf("Ping to gateway failed\n");
    return false;
}

bool check_connectivity_via_dns() {
    const ip_addr_t *dns_ip = dns_getserver(0);
    if (ip_addr_isany(dns_ip)) {
        printf("No DNS server assigned by DHCP.\n");
        return true;
    }

    char ip_str[IPADDR_STRLEN_MAX];
    ipaddr_ntoa_r(dns_ip, ip_str, sizeof(ip_str));
    printf("Pinging DNS server at %s\n", ip_str);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("Socket creation failed.\n");
        return false;
    }

    struct sockaddr_in addr = {.sin_family = AF_INET,
                               .sin_port = htons(53), // DNS port
                               .sin_addr.s_addr =
                                   ip4_addr_get_u32(ip_2_ip4(dns_ip))};

    int result = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    close(sock);

    return result == 0;
}

void network_manager_task(void *params) {
    printf("NetworkManager: starting...\n");

    if (network_platform_init()) {
        printf("NetworkManager: Wi-Fi init failed\n");
        vTaskDelete(NULL);
    }

    network_config_t cfg;
    if (load_network_config(&cfg) < 0) {
        printf("NetworkManager: failed to load embedded config\n");
        vTaskDelete(NULL);
    }

    network_platform_enable_sta_mode();

    while (true) {

        if (!connected) {
            // I will try to connect
            if (!scan_in_progress) {

                for (int i = 0; i < cfg.wifi_list_size && !connected; ++i) {
                    connected = try_connect_wifi(&cfg.wifi_list[i]);
                    if (connected) {
                        struct netif *netif = netif_list;
                        ip_addr_t ip = netif->ip_addr;

                        network_event_t *evt = malloc(sizeof(network_event_t));
                        if (!evt) {
                            printf("Failed to allocate memory for network "
                                   "event\n");
                            vTaskDelete(NULL);
                        }
                        evt->type = EVENT_WIFI_CONNECTED;
                        memset(evt->ssid, 0, 33); // Clear SSID
                        strncpy(evt->ssid, cfg.wifi_list[i].ssid, 32);
                        memset(evt->ip, 0, 16); // Clear IP
#ifdef BUILD_PICO
                        ip4addr_ntoa_r(&ip, evt->ip, 15);
#elif defined(BUILD_ESP32)
                        strcpy(evt->ip, ip4addr_ntoa(ip_2_ip4(&ip)));
#endif

                        event_t *event = create_event(
                            EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_UP,
                            evt, sizeof(network_event_t *));

                        if (event != NULL) {
                            event_dispatcher_post(event);
                        } else {
                            printf("Failed to create network event\n");
                            free(evt);
                        }

#ifdef BUILD_PICO
                        printf("[EVENT] Connected to %s | IP: %s\n",
                               cfg.wifi_list[i].ssid, ip4addr_ntoa(&ip));
#elif defined(BUILD_ESP32)
                        printf("[EVENT] Connected to %s | IP: %s\n",
                               cfg.wifi_list[i].ssid,
                               ip4addr_ntoa(ip_2_ip4(&ip)));
#endif
                    }
                }
            }

            if (!connected) {
                printf("NetworkManager: failed to connect to any configured "
                       "network\n");

                scan_for_open_networks();

                if (scan_has_completed) {
                    open_network_info_t *current = open_networks;
                    while (!connected && current != NULL) {
                        printf("Trying to connect to open network: %s\n",
                               current->ssid);
                        if (network_platform_connect_wifi(current->ssid, NULL,
                                                          10000) == 0) {
                            connected = true;
                            struct netif *netif = netif_list;
                            ip_addr_t ip = netif->ip_addr;

                            network_event_t *evt =
                                malloc(sizeof(network_event_t));
                            if (!evt) {
                                printf("Failed to allocate memory for network "
                                       "event\n");
                                vTaskDelete(NULL);
                            }
                            evt->type = EVENT_WIFI_CONNECTED;
                            memset(evt->ssid, 0, 33); // Clear SSID
                            strncpy(evt->ssid, current->ssid,
                                    strlen(current->ssid));
                            memset(evt->ip, 0, 16); // Clear IP
#ifdef BUILD_PICO
                            ip4addr_ntoa_r(&ip, evt->ip, 15);
#elif defined(BUILD_ESP32)
                            strcpy(evt->ip, ip4addr_ntoa(ip_2_ip4(&ip)));
#endif

                            event_t *event =
                                create_event(EVENT_TYPE_NOTIFICATION,
                                             EVENT_SUBTYPE_NETWORK_UP, evt,
                                             sizeof(network_event_t *));

                            if (event != NULL) {
                                event_dispatcher_post(event);
                            } else {
                                printf("Failed to create network event\n");
                                free(evt);
                            }

#ifdef BUILD_PICO
                            printf("[EVENT] Connected to open network %s | IP: "
                                   "%s\n",
                                   current->ssid, ip4addr_ntoa(&ip));
#elif defined(BUILD_ESP32)
                            printf("[EVENT] Connected to %s | IP: %s\n",
                                   current->ssid, ip4addr_ntoa(ip_2_ip4(&ip)));
#endif
                        } else {
                            current = current->next;
                        }
                    }

                    if (open_networks != NULL) {
                        free_open_networks();
                    }
                    scan_in_progress = false;
                    scan_has_completed = false;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            // Monitor link status check
            int curr_status = network_platform_link_status();

            if (curr_status != 1 || (!check_connectivity_via_ping_gateway() && !check_connectivity_via_dns())) {
                printf("[EVENT] Wi-Fi link lost.\n");

                network_event_t *evt = malloc(sizeof(network_event_t));
                if (!evt) {
                    printf("Failed to allocate memory for network event\n");
                    vTaskDelete(NULL);
                }
                evt->type = EVENT_WIFI_DISCONNECTED;
                evt->ssid[0] = '\0'; // Clear SSID
                evt->ip[0] = '\0';   // Clear IP

                event_t *event = create_event(EVENT_TYPE_NOTIFICATION,
                                              EVENT_SUBTYPE_NETWORK_DOWN, evt,
                                              sizeof(network_event_t));

                if (event != NULL) {
                    event_dispatcher_post(event);
                } else {
                    printf("Failed to create network event\n");
                    free(evt);
                }

                network_platform_disconnect_wifi();
                connected = false;
                // emit or handle evt here
                printf("[EVENT] Disconnected from Wi-Fi.\n");
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}
