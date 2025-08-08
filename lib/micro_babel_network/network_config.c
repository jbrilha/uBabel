// network_config.c
#include "network_config.h"
#include <string.h>

int load_network_config(network_config_t* config) {
    config->wifi_list_size = 1;

    strncpy(config->wifi_list[0].ssid, "TaRDIS-LAB", MAX_SSID_LEN);
    strncpy(config->wifi_list[0].pass, "TaRDISTaRDIS", MAX_PASS_LEN);

    return config->wifi_list_size;
}
/** 
int load_network_config(network_config_t* config) {
    config->wifi_list_size = 2;

    strncpy(config->wifi_list[0].ssid, "TaRDIS-LAB", MAX_SSID_LEN);
    strncpy(config->wifi_list[0].pass, "TaRDISTaRDIS", MAX_PASS_LEN);

    strncpy(config->wifi_list[1].ssid, "OpenFCT", MAX_SSID_LEN);
    config->wifi_list[1].pass[0] = '\0'; // Open network

    return config->wifi_list_size;
}
*/