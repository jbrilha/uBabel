#ifndef GATT_SERVER_H
#define GATT_SERVER_H

#include <stdbool.h>

struct ble_hs_cfg;
struct ble_gatt_register_ctxt;

#define GATTS_SVC_ALERT_UUID               0x1811
#define GATTS_CHR_SUP_NEW_ALERT_CAT_UUID   0x2A47
#define GATTS_CHR_NEW_ALERT                0x2A46
#define GATTS_CHR_SUP_UNR_ALERT_CAT_UUID   0x2A48
#define GATTS_CHR_UNR_ALERT_STAT_UUID      0x2A45
#define GATTS_CHR_ALERT_NOT_CTRL_PT        0x2A44

void gatt_server_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
int gatt_server_init(void);

#endif
