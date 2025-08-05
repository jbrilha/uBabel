// network_config.h
#pragma once

#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64
#define MAX_NETWORKS 10

typedef struct {
    char ssid[MAX_SSID_LEN + 1];
    char pass[MAX_PASS_LEN + 1];
} wifi_credentials_t;

typedef struct {
    wifi_credentials_t wifi_list[MAX_NETWORKS];
    int wifi_list_size;
} network_config_t;

int load_network_config(network_config_t* config);