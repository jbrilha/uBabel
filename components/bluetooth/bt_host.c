#include "bt_host.h"
#include "ble_peripheral.h"
#include "ble_central.h"
#include "ble_utils.h"
#include "gatt_server.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"

#include <assert.h>

static const char *TAG = "BLUETOOTH";

static uint8_t own_addr_type;
void ble_store_config_init(void);
static uint8_t addr_val[6] = {0};

static void ble_on_reset(int reason) {
    ESP_LOGW(TAG, "Resetting state; reason=%d", reason);
}

void ble_host_task(void *param) {
    ESP_LOGI(TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

esp_err_t bt_host_init_as_central(void) {
    esp_err_t ret;

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init nimble %d ", ret);
        return ret;
    }

    /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.sync_cb = ble_central_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    int rc;
#if NIMBLE_BLE_CONNECT
    /* Initialize data structures to track connected peers. */
#if MYNEWT_VAL(BLE_INCL_SVC_DISCOVERY) ||                                      \
    MYNEWT_VAL(BLE_GATT_CACHING_INCLUDE_SERVICES)
    rc = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64, 64);
    assert(rc == 0);
#else
    rc = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
    assert(rc == 0);
#endif
#endif

#if CONFIG_BT_NIMBLE_GAP_SERVICE
    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("ubabel-ble-central");
    assert(rc == 0);
#endif

    /* XXX Need to have template for store */
    ble_store_config_init();

    nimble_port_freertos_init(ble_host_task);

    return ESP_OK;
}

esp_err_t bt_host_init_as_peripheral(void) {
    esp_err_t ret;

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init nimble %d ", ret);
        return ret;
    }

    /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.sync_cb = ble_peripheral_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_server_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    int rc;
#if MYNEWT_VAL(BLE_GATTS)
    rc = gatt_server_init();
    assert(rc == 0);
#endif

#if CONFIG_BT_NIMBLE_GAP_SERVICE
    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("ubabel-ble-prphrl");
    assert(rc == 0);
#endif

    /* XXX Need to have template for store */
    ble_store_config_init();

    nimble_port_freertos_init(ble_host_task);

    return ESP_OK;
}
