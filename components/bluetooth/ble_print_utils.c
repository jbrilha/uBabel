#include "ble_utils.h"

#include "esp_log.h"
#include <stdint.h>
#include <stdio.h>

static const char *TAG = "BLE_UTILS";

void ble_print_bytes(const uint8_t *bytes, int len) {
    int i;

    for (i = 0; i < len; i++) {
        ESP_LOGI(TAG, "%s0x%02x", i != 0 ? ":" : "", bytes[i]);
    }
}

static void ble_print_addr(const void *addr) {
    const uint8_t *u8p;

    u8p = addr;
    ESP_LOGI(TAG, "%02x:%02x:%02x:%02x:%02x:%02x", u8p[5], u8p[4], u8p[3],
             u8p[2], u8p[1], u8p[0]);
}

static void ble_print_addr_name(const void *addr, const char *name) {
    const uint8_t *u8p;
    u8p = addr;
    ESP_LOGI(TAG, "%s = %02x:%02x:%02x:%02x:%02x:%02x", name, u8p[5], u8p[4],
             u8p[3], u8p[2], u8p[1], u8p[0]);
}

void ble_print_ext_adv_report(const void *param) {
    const struct ble_gap_ext_disc_desc *disc =
        (struct ble_gap_ext_disc_desc *)param;
    ESP_LOGI(TAG, "props=%d data_status=%d legacy_event_type=%d", disc->props,
             disc->data_status, disc->legacy_event_type);
    ble_print_addr_name(disc->addr.val, "address");
    ESP_LOGI(TAG, "rssi=%d tx_power=%d", disc->rssi, disc->tx_power);
    ESP_LOGI(TAG, "sid=%d prim_phy=%d sec_phy=%d", disc->sid, disc->prim_phy,
             disc->sec_phy);
    ESP_LOGI(TAG, "periodic_adv_itvl=%d length_data=%d",
             disc->periodic_adv_itvl, disc->length_data);
    ble_print_addr_name(disc->direct_addr.val, "direct address");
}

char *addr_str(const void *addr) {
    static char buf[6 * 2 + 5 + 1];
    const uint8_t *u8p;

    u8p = addr;
    sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", u8p[5], u8p[4], u8p[3],
            u8p[2], u8p[1], u8p[0]);

    return buf;
}

void ble_print_uuid(const ble_uuid_t *uuid) {
    char buf[BLE_UUID_STR_LEN];

    ESP_LOGI(TAG, "%s", ble_uuid_to_str(uuid, buf));
}

void ble_print_conn_desc(const struct ble_gap_conn_desc *desc) {
    ESP_LOGI(TAG,
             "handle=%d our_ota_addr_type=%d our_ota_addr=", desc->conn_handle,
             desc->our_ota_addr.type);
    ble_print_addr(desc->our_ota_addr.val);
    ESP_LOGI(TAG, " our_id_addr_type=%d our_id_addr=", desc->our_id_addr.type);
    ble_print_addr(desc->our_id_addr.val);
    ESP_LOGI(TAG,
             " peer_ota_addr_type=%d peer_ota_addr=", desc->peer_ota_addr.type);
    ble_print_addr(desc->peer_ota_addr.val);
    ESP_LOGI(TAG,
             " peer_id_addr_type=%d peer_id_addr=", desc->peer_id_addr.type);
    ble_print_addr(desc->peer_id_addr.val);
    ESP_LOGI(TAG,
             " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
             "encrypted=%d authenticated=%d bonded=%d\n",
             desc->conn_itvl, desc->conn_latency, desc->supervision_timeout,
             desc->sec_state.encrypted, desc->sec_state.authenticated,
             desc->sec_state.bonded);
}

void ble_print_adv_fields(const struct ble_hs_adv_fields *fields) {
    char s[BLE_HS_ADV_MAX_SZ];
    const uint8_t *u8p;
    int i;

    if (fields->flags != 0) {
        ESP_LOGI(TAG, "    flags=0x%02x\n", fields->flags);
    }

    if (fields->uuids16 != NULL) {
        ESP_LOGI(TAG, "    uuids16(%scomplete)=",
                 fields->uuids16_is_complete ? "" : "in");
        for (i = 0; i < fields->num_uuids16; i++) {
            ble_print_uuid(&fields->uuids16[i].u);
            ESP_LOGI(TAG, " ");
        }
        ESP_LOGI(TAG, "\n");
    }

    if (fields->uuids32 != NULL) {
        ESP_LOGI(TAG, "    uuids32(%scomplete)=",
                 fields->uuids32_is_complete ? "" : "in");
        for (i = 0; i < fields->num_uuids32; i++) {
            ble_print_uuid(&fields->uuids32[i].u);
            ESP_LOGI(TAG, " ");
        }
        ESP_LOGI(TAG, "\n");
    }

    if (fields->uuids128 != NULL) {
        ESP_LOGI(TAG, "    uuids128(%scomplete)=",
                 fields->uuids128_is_complete ? "" : "in");
        for (i = 0; i < fields->num_uuids128; i++) {
            ble_print_uuid(&fields->uuids128[i].u);
            ESP_LOGI(TAG, " ");
        }
        ESP_LOGI(TAG, "\n");
    }

    if (fields->name != NULL) {
        assert(fields->name_len < sizeof s - 1);
        memcpy(s, fields->name, fields->name_len);
        s[fields->name_len] = '\0';
        ESP_LOGI(TAG, "    name(%scomplete)=%s\n",
                 fields->name_is_complete ? "" : "in", s);
    }

    if (fields->tx_pwr_lvl_is_present) {
        ESP_LOGI(TAG, "    tx_pwr_lvl=%d\n", fields->tx_pwr_lvl);
    }

    if (fields->slave_itvl_range != NULL) {
        ESP_LOGI(TAG, "    slave_itvl_range=");
        ble_print_bytes(fields->slave_itvl_range,
                        BLE_HS_ADV_SLAVE_ITVL_RANGE_LEN);
        ESP_LOGI(TAG, "\n");
    }

    if (fields->sm_tk_value_is_present) {
        ESP_LOGI(TAG, "    sm_tk_value=");
        ble_print_bytes(fields->sm_tk_value, 16);
        ESP_LOGI(TAG, "\n");
    }

    if (fields->sm_oob_flag_is_present) {
        ESP_LOGI(TAG, "    sm_oob_flag=%d\n", fields->sm_oob_flag);
    }

    if (fields->sol_uuids16 != NULL) {
        ESP_LOGI(TAG, "    sol_uuids16=");
        for (i = 0; i < fields->sol_num_uuids16; i++) {
            ble_print_uuid(&fields->sol_uuids16[i].u);
            ESP_LOGI(TAG, " ");
        }
        ESP_LOGI(TAG, "\n");
    }

    if (fields->sol_uuids32 != NULL) {
        ESP_LOGI(TAG, "    sol_uuids32=");
        for (i = 0; i < fields->sol_num_uuids32; i++) {
            ble_print_uuid(&fields->sol_uuids32[i].u);
            ESP_LOGI(TAG, "\n");
        }
        ESP_LOGI(TAG, "\n");
    }

    if (fields->sol_uuids128 != NULL) {
        ESP_LOGI(TAG, "    sol_uuids128=");
        for (i = 0; i < fields->sol_num_uuids128; i++) {
            ble_print_uuid(&fields->sol_uuids128[i].u);
            ESP_LOGI(TAG, " ");
        }
        ESP_LOGI(TAG, "\n");
    }

    if (fields->svc_data_uuid16 != NULL) {
        ESP_LOGI(TAG, "    svc_data_uuid16=");
        ble_print_bytes(fields->svc_data_uuid16, fields->svc_data_uuid16_len);
        ESP_LOGI(TAG, "\n");
    }

    if (fields->public_tgt_addr != NULL) {
        ESP_LOGI(TAG, "    public_tgt_addr=");
        u8p = fields->public_tgt_addr;
        for (i = 0; i < fields->num_public_tgt_addrs; i++) {
            ESP_LOGI(TAG, "public_tgt_addr=%s ", addr_str(u8p));
            u8p += BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN;
        }
        ESP_LOGI(TAG, "\n");
    }

    if (fields->random_tgt_addr != NULL) {
        ESP_LOGI(TAG, "    random_tgt_addr=");
        u8p = fields->random_tgt_addr;
        for (i = 0; i < fields->num_random_tgt_addrs; i++) {
            ESP_LOGI(TAG, "random_tgt_addr=%s ", addr_str(u8p));
            u8p += BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN;
        }
        ESP_LOGI(TAG, "\n");
    }

    if (fields->appearance_is_present) {
        ESP_LOGI(TAG, "    appearance=0x%04x\n", fields->appearance);
    }

    if (fields->adv_itvl_is_present) {
        ESP_LOGI(TAG, "    adv_itvl=0x%04x\n", fields->adv_itvl);
    }

    if (fields->device_addr_is_present) {
        ESP_LOGI(TAG, "    device_addr=");
        u8p = fields->device_addr;
        ESP_LOGI(TAG, "%s ", addr_str(u8p));

        u8p += BLE_HS_ADV_PUBLIC_TGT_ADDR_ENTRY_LEN;
        ESP_LOGI(TAG, "addr_type %d ", *u8p);
    }

    if (fields->le_role_is_present) {
        ESP_LOGI(TAG, "    le_role=%d\n", fields->le_role);
    }

    if (fields->svc_data_uuid32 != NULL) {
        ESP_LOGI(TAG, "    svc_data_uuid32=");
        ble_print_bytes(fields->svc_data_uuid32, fields->svc_data_uuid32_len);
        ESP_LOGI(TAG, "\n");
    }

    if (fields->svc_data_uuid128 != NULL) {
        ESP_LOGI(TAG, "    svc_data_uuid128=");
        ble_print_bytes(fields->svc_data_uuid128, fields->svc_data_uuid128_len);
        ESP_LOGI(TAG, "\n");
    }

    if (fields->uri != NULL) {
        ESP_LOGI(TAG, "    uri=");
        ble_print_bytes(fields->uri, fields->uri_len);
        ESP_LOGI(TAG, "\n");
    }

    if (fields->mfg_data != NULL) {
        ESP_LOGI(TAG, "    mfg_data=");
        ble_print_bytes(fields->mfg_data, fields->mfg_data_len);
        ESP_LOGI(TAG, "\n");
    }
}

void ble_print_mbuf(const struct os_mbuf *om) {
    int colon, i;

    colon = 0;
    while (om != NULL) {
        if (colon) {
            ESP_LOGI(TAG, ":");
        } else {
            colon = 1;
        }
        for (i = 0; i < om->om_len; i++) {
            ESP_LOGI(TAG, "%s0x%02x", i != 0 ? ":" : "", om->om_data[i]);
        }
        om = SLIST_NEXT(om, om_next);
    }
}
