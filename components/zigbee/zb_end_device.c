#include "zb_end_device.h"
#include "esp_check.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "light_driver.h"
#include "ubabel_zigbee_proto.h"
#include "zb_coordinator.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "zcl_utility.h"
#include "zigbee.h"
#include <stdint.h>

#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE in idf.py menuconfig to compile light (End Device) source code.
#endif

static const char *TAG = "ZB_END_DEV";

static esp_err_t deferred_driver_init(void) {
    light_driver_init(LIGHT_DEFAULT_OFF);
    return ESP_OK;
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) ==
                            ESP_OK,
                        , TAG, "Failed to start Zigbee commissioning");
}

// this function is declared in "esp_zigbee_core" but needs to be implemented
// here
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) {
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(
            ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Deferred driver initialization %s",
                     deferred_driver_init() ? "failed" : "successful");
            ESP_LOGI(TAG, "Device started up in %s factory-reset mode",
                     esp_zb_bdb_is_factory_new() ? "" : "non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(
                    ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted");
            }
        } else {
            /* commissioning failed */
            ESP_LOGW(TAG, "Failed to initialize Zigbee stack (status: %s)",
                     esp_err_to_name(err_status));
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG,
                     "Joined network successfully (Extended PAN ID: "
                     "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: "
                     "0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5],
                     extended_pan_id[4], extended_pan_id[3], extended_pan_id[2],
                     extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(),
                     esp_zb_get_short_address());
        } else {
            ESP_LOGI(TAG, "Network steering was not successful (status: %s)",
                     esp_err_to_name(err_status));
            esp_zb_scheduler_alarm(
                (esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s",
                 esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}

void ubabel_send_sensor_reading(uint16_t value) {
    esp_zb_zcl_attribute_t attr = {
        .id = UBABEL_ATTR_SENSOR_READING_ID,
        .data =
            {
                .type = ESP_ZB_ZCL_ATTR_TYPE_U16,
                .size = sizeof(uint16_t),
                .value = &value,
            },
    };

    esp_zb_zcl_write_attr_cmd_t cmd = {
        .zcl_basic_cmd =
            {
                .src_endpoint = HA_ESP_LIGHT_ENDPOINT,
                .dst_endpoint = HA_ONOFF_SWITCH_ENDPOINT,
                .dst_addr_u.addr_short =
                    0x0000, // TODO receive and store this somewhere
            },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID = UBABEL_CUSTOM_CLUSTER_ID,
        .attr_number = 1,
        .attr_field = &attr,
    };

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_write_attr_cmd_req(&cmd);
    esp_zb_lock_release();
}

static esp_err_t
zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message) {
    esp_err_t ret = ESP_OK;
    bool light_state = 0;

    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(
        message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG,
        TAG, "Received message: error status(%d)", message->info.status);
    ESP_LOGI(TAG,
             "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), "
             "data size(%d)",
             message->info.dst_endpoint, message->info.cluster,
             message->attribute.id, message->attribute.data.size);
    if (message->info.dst_endpoint == HA_ESP_LIGHT_ENDPOINT) {
        if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID &&
                message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
                light_state = message->attribute.data.value
                                  ? *(bool *)message->attribute.data.value
                                  : light_state;
                ESP_LOGI(TAG, "Light sets to %s", light_state ? "On" : "Off");
                light_driver_set_power(light_state);
            }
        }
    }

    if (message->info.cluster == UBABEL_CUSTOM_CLUSTER_ID) {
        if (message->attribute.id == UBABEL_ATTR_SENSOR_READING_ID) {
            uint16_t reading = *(uint16_t *)message->attribute.data.value;
            ESP_LOGI(TAG, "Sensor reading on endpoint(%d): %d",
                     message->info.dst_endpoint, reading);
        }
    }
    return ret;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id,
                                   const void *message) {
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zb_attribute_handler(
            (esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

static void zb_sensor(void *params) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    for (uint16_t i = 0; i < UINT16_MAX; i++) {
        ubabel_send_sensor_reading(i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void zb_ed_task(void *params) {
    /* initialize Zigbee stack */
    esp_zb_cfg_t zb_nwk_cfg = ZB_END_DEV_CONFIG();
    esp_zb_init(&zb_nwk_cfg);
    esp_zb_on_off_light_cfg_t light_cfg = ESP_ZB_DEFAULT_ON_OFF_LIGHT_CONFIG();
    esp_zb_ep_list_t *esp_zb_on_off_light_ep =
        esp_zb_on_off_light_ep_create(HA_ESP_LIGHT_ENDPOINT, &light_cfg);

    esp_zb_attribute_list_t *custom_cluster =
        esp_zb_zcl_attr_list_create(UBABEL_CUSTOM_CLUSTER_ID);
    uint16_t sensor_val = 0;
    esp_zb_custom_cluster_add_custom_attr(
        custom_cluster, UBABEL_ATTR_SENSOR_READING_ID, ESP_ZB_ZCL_ATTR_TYPE_U16,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE, &sensor_val);

    esp_zb_cluster_list_t *cluster_list =
        esp_zb_ep_list_get_ep(esp_zb_on_off_light_ep, HA_ESP_LIGHT_ENDPOINT);
    esp_zb_cluster_list_add_custom_cluster(cluster_list, custom_cluster,
                                           ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);

    zcl_basic_manufacturer_info_t info = {
        .manufacturer_name = ESP_MANUFACTURER_NAME,
        .model_identifier = ESP_MODEL_IDENTIFIER,
    };

    esp_zcl_utility_add_ep_basic_manufacturer_info(
        esp_zb_on_off_light_ep, HA_ESP_LIGHT_ENDPOINT, &info);
    esp_zb_device_register(esp_zb_on_off_light_ep);
    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_primary_network_channel_set(ZB_ED_PRIMARY_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}
/* static void zb_ed_task(void *params) {
    esp_zb_cfg_t zb_nwk_cfg = ZB_END_DEV_CONFIG();
    esp_zb_init(&zb_nwk_cfg);
    esp_zb_on_off_light_cfg_t light_cfg = ESP_ZB_DEFAULT_ON_OFF_LIGHT_CONFIG();
    esp_zb_ep_list_t *esp_zb_on_off_light_ep =
        esp_zb_on_off_light_ep_create(HA_ESP_LIGHT_ENDPOINT, &light_cfg);
    zcl_basic_manufacturer_info_t info = {
        .manufacturer_name = ESP_MANUFACTURER_NAME,
        .model_identifier = ESP_MODEL_IDENTIFIER,
    };

    esp_zcl_utility_add_ep_basic_manufacturer_info(esp_zb_on_off_light_ep,
                                                   HA_ESP_LIGHT_ENDPOINT,
&info); esp_zb_device_register(esp_zb_on_off_light_ep);
    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_primary_network_channel_set(ZB_ED_PRIMARY_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}
*/

void zigbee_start(void) {
    if (!zigbee_platform_init()) {
        return;
    }

    ESP_LOGI(TAG, "Starting ZigBee as end device");

    xTaskCreate(zb_ed_task, "ZB_end_device", 4096, NULL, 5, NULL);
    xTaskCreate(zb_sensor, "ZB_reading", 4096, NULL, 5, NULL);
}
