#ifndef BLE_CENTRAL_H
#define BLE_CENTRAL_H

struct ble_hs_adv_fields;
struct ble_gap_conn_desc;
struct ble_hs_cfg;
union ble_store_value;
union ble_store_key;

#define BLE_CENTRAL_SVC_ALERT_UUID 0x1811
#define BLE_CENTRAL_CHR_SUP_NEW_ALERT_CAT_UUID 0x2A47
#define BLE_CENTRAL_CHR_NEW_ALERT 0x2A46
#define BLE_CENTRAL_CHR_SUP_UNR_ALERT_CAT_UUID 0x2A48
#define BLE_CENTRAL_CHR_UNR_ALERT_STAT_UUID 0x2A45
#define BLE_CENTRAL_CHR_ALERT_NOT_CTRL_PT 0x2A44

void ble_central_on_sync(void);

#endif
