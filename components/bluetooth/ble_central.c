#include "ble_central.h"
#include "ble_utils.h"

#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/util/util.h"

/*** The UUID of the service containing the subscribable characteristic ***/
static const ble_uuid_t *remote_svc_uuid =
    BLE_UUID128_DECLARE(0x2d, 0x71, 0xa2, 0x59, 0xb4, 0x58, 0xc8, 0x12, 0x99,
                        0x99, 0x43, 0x95, 0x12, 0x2f, 0x46, 0x59);

/*** The UUID of the subscribable chatacteristic ***/
static const ble_uuid_t *remote_chr_uuid =
    BLE_UUID128_DECLARE(0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x22,
                        0x22, 0x22, 0x22, 0x33, 0x33, 0x33, 0x33);

static const char *TAG = "BLE_CENTRAL";

#define PEER_ADDR "ADDR_ANY"

static int ble_central_gap_event(struct ble_gap_event *event, void *arg);

#if CONFIG_UBABEL_BLE_EXTENDED_ADV
static int
ext_ble_central_should_connect(const struct ble_gap_ext_disc_desc *disc) {
    int offset = 0;
    int ad_struct_len = 0;
#if CONFIG_UBABEL_BLE_USE_CI_ADDRESS
    uint32_t *addr_offset;
#endif // CONFIG_UBABEL_BLE_USE_CI_ADDRESS
    uint8_t test_addr[6];
    uint32_t peer_addr[6];

    memset(peer_addr, 0x0, sizeof peer_addr);

    if (disc->legacy_event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
        disc->legacy_event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {
        return 0;
    }
    if (strlen(PEER_ADDR) &&
        (strncmp(PEER_ADDR, "ADDR_ANY", strlen("ADDR_ANY")) != 0)) {
#if !CONFIG_UBABEL_BLE_USE_CI_ADDRESS
        ESP_LOGI(TAG, "Peer address from menuconfig: %s", PEER_ADDR);
        /* Convert string to address */
        sscanf(PEER_ADDR, "%lx:%lx:%lx:%lx:%lx:%lx", &peer_addr[5],
               &peer_addr[4], &peer_addr[3], &peer_addr[2], &peer_addr[1],
               &peer_addr[0]);
#endif

        /* Conversion */
        for (int i = 0; i < 6; i++) {
            test_addr[i] = (uint8_t)peer_addr[i];
        }

#if CONFIG_UBABEL_BLE_USE_CI_ADDRESS
        addr_offset = (uint32_t *)&test_addr[1];
        *addr_offset = atoi(PEER_ADDR);
        test_addr[5] = 0xC3;
        test_addr[0] = TEST_CI_ADDRESS_CHIP_OFFSET;
#endif
        if (memcmp(test_addr, disc->addr.val, sizeof(disc->addr.val)) != 0) {
            return 0;
        }
    }

    /* The device has to advertise support for the Alert Notification
     * service (0x1811).
     */
    do {
        ad_struct_len = disc->data[offset];

        if (!ad_struct_len) {
            break;
        }

        /* Search if ANS UUID is advertised */
        if (disc->data[offset] == 0x03 && disc->data[offset + 1] == 0x03) {
            if (disc->data[offset + 2] == 0x18 &&
                disc->data[offset + 3] == 0x11) {
                return 1;
            }
        }

        offset += ad_struct_len + 1;

    } while (offset < disc->length_data);

    return 0;
}
#else
static int ble_central_should_connect(const struct ble_gap_disc_desc *disc) {
    struct ble_hs_adv_fields fields;
    int rc;
    int i;
#if CONFIG_UBABEL_BLE_USE_CI_ADDRESS
    uint32_t *addr_offset;
#endif // CONFIG_UBABEL_BLE_USE_CI_ADDRESS
    uint8_t test_addr[6];
    uint32_t peer_addr[6];

    memset(peer_addr, 0x0, sizeof peer_addr);

    /* The device has to be advertising connectability. */
    if (disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
        disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {

        return 0;
    }

    rc = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data);
    if (rc != 0) {
        return 0;
    }

    if (strlen(PEER_ADDR) &&
        (strncmp(PEER_ADDR, "ADDR_ANY", strlen("ADDR_ANY")) != 0)) {
        ESP_LOGI(TAG, "Peer address from menuconfig: %s", PEER_ADDR);
#if !CONFIG_UBABEL_BLE_USE_CI_ADDRESS
        /* Convert string to address */
        sscanf(PEER_ADDR, "%lx:%lx:%lx:%lx:%lx:%lx", &peer_addr[5],
               &peer_addr[4], &peer_addr[3], &peer_addr[2], &peer_addr[1],
               &peer_addr[0]);
        printf("peer-->  %lx %lx %lx %lx %lx %lx \n", peer_addr[5],
               peer_addr[4], peer_addr[3], peer_addr[2], peer_addr[1],
               peer_addr[0]);
#endif
        /* Conversion */
        for (int i = 0; i < 6; i++) {
            test_addr[i] = (uint8_t)peer_addr[i];
        }

#if CONFIG_UBABEL_BLE_USE_CI_ADDRESS
        addr_offset = (uint32_t *)&test_addr[1];
        *addr_offset = atoi(PEER_ADDR);
        test_addr[5] = 0xC3;
        test_addr[0] = TEST_CI_ADDRESS_CHIP_OFFSET;
#endif

        if (memcmp(test_addr, disc->addr.val, sizeof(disc->addr.val)) != 0) {
            return 0;
        }
    }

    /* The device has to advertise support for the Alert Notification
     * service (0x1811).
     */
    for (i = 0; i < fields.num_uuids16; i++) {
        if (ble_uuid_u16(&fields.uuids16[i].u) == BLE_CENTRAL_SVC_ALERT_UUID) {
            return 1;
        }
    }

    return 0;
}
#endif

/**
 * Connects to the sender of the specified advertisement of it looks
 * interesting.  A device is "interesting" if it advertises connectability and
 * support for the Alert Notification service.
 */
static void ble_central_connect_if_interesting(void *disc) {
    uint8_t own_addr_type;
    int rc;
    ble_addr_t *addr;

    /* Don't do anything if we don't care about this advertiser. */
#if CONFIG_UBABEL_BLE_EXTENDED_ADV
    if (!ext_ble_central_should_connect((struct ble_gap_ext_disc_desc *)disc)) {
        return;
    }
#else
    if (!ble_central_should_connect((struct ble_gap_disc_desc *)disc)) {
        return;
    }
#endif

#if !(MYNEWT_VAL(BLE_HOST_ALLOW_CONNECT_WITH_SCAN))
    /* Scanning must be stopped before a connection can be initiated. */
    rc = ble_gap_disc_cancel();
    if (rc != 0) {
        ESP_LOGI(TAG, "Failed to cancel scan; rc=%d\n", rc);
        return;
    }
#endif

    /* Figure out address to use for connect (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Try to connect the the advertiser.  Allow 30 seconds (30000 ms) for
     * timeout.
     */
#if CONFIG_UBABEL_BLE_EXTENDED_ADV
    addr = &((struct ble_gap_ext_disc_desc *)disc)->addr;
#else
    addr = &((struct ble_gap_disc_desc *)disc)->addr;
#endif

    rc = ble_gap_connect(own_addr_type, addr, 30000, NULL,
                         ble_central_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG,
                 "Error: Failed to connect to device; addr_type=%d "
                 "addr=%s; rc=%d\n",
                 addr->type, addr_str(addr->val), rc);
        return;
    }
}

/**
 * Application Callback. Called when the custom subscribable chatacteristic
 * in the remote GATT server is read.
 * Expect to get the recently written data.
 **/
static int ble_central_on_custom_read(uint16_t conn_handle,
                                      const struct ble_gatt_error *error,
                                      struct ble_gatt_attr *attr, void *arg) {
    MODLOG_DFLT(INFO,
                "Read complete for the subscribable characteristic; "
                "status=%d conn_handle=%d",
                error->status, conn_handle);
    if (error->status == 0) {
        MODLOG_DFLT(INFO, " attr_handle=%d value=", attr->handle);
        ble_print_mbuf(attr->om);
    }
    MODLOG_DFLT(INFO, "\n");

    return 0;
}

/**
 * Application Callback. Called when the custom subscribable characteristic
 * in the remote GATT server is written to.
 * Client has previously subscribed to this characeteristic,
 * so expect a notification from the server.
 **/
static int ble_central_on_custom_write(uint16_t conn_handle,
                                       const struct ble_gatt_error *error,
                                       struct ble_gatt_attr *attr, void *arg) {
    const struct peer_chr *chr;
    const struct peer *peer;
    int rc;

    MODLOG_DFLT(INFO,
                "Write to the custom subscribable characteristic complete; "
                "status=%d conn_handle=%d attr_handle=%d\n",
                error->status, conn_handle, attr->handle);

    peer = peer_find(conn_handle);
    chr = peer_chr_find_uuid(peer, remote_svc_uuid, remote_chr_uuid);
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't have the custom subscribable "
                           "characteristic\n");
        goto err;
    }

    /*** Performs a read on the characteristic, the result is handled in
     * ble_central_on_new_read callback ***/
    rc = ble_gattc_read(conn_handle, chr->chr.val_handle,
                        ble_central_on_custom_read, NULL);
    if (rc != 0) {
        MODLOG_DFLT(
            ERROR,
            "Error: Failed to read the custom subscribable characteristic; "
            "rc=%d\n",
            rc);
        goto err;
    }

    return 0;
err:
    /* Terminate the connection */
    return ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Application Callback. Called when the custom subscribable characteristic
 * is subscribed to.
 **/
static int ble_central_on_custom_subscribe(uint16_t conn_handle,
                                           const struct ble_gatt_error *error,
                                           struct ble_gatt_attr *attr,
                                           void *arg) {
    const struct peer_chr *chr;
    uint8_t value;
    int rc;
    const struct peer *peer;

    MODLOG_DFLT(INFO,
                "Subscribe to the custom subscribable characteristic complete; "
                "status=%d conn_handle=%d",
                error->status, conn_handle);

    if (error->status == 0) {
        MODLOG_DFLT(INFO, " attr_handle=%d value=", attr->handle);
        ble_print_mbuf(attr->om);
    }
    MODLOG_DFLT(INFO, "\n");

    peer = peer_find(conn_handle);
    chr = peer_chr_find_uuid(peer, remote_svc_uuid, remote_chr_uuid);
    if (chr == NULL) {
        MODLOG_DFLT(
            ERROR,
            "Error: Peer doesn't have the subscribable characteristic\n");
        goto err;
    }

    /* Write 1 byte to the new characteristic to test if it notifies after
     * subscribing */
    value = 0x19;
    rc = ble_gattc_write_flat(conn_handle, chr->chr.val_handle, &value,
                              sizeof(value), ble_central_on_custom_write, NULL);
    if (rc != 0) {
        MODLOG_DFLT(
            ERROR,
            "Error: Failed to write to the subscribable characteristic; "
            "rc=%d\n",
            rc);
        goto err;
    }

    return 0;
err:
    /* Terminate the connection */
    return ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Performs 3 operations on the remote GATT server.
 * 1. Subscribes to a characteristic by writing 0x10 to it's CCCD.
 * 2. Writes to the characteristic and expect a notification from remote.
 * 3. Reads the characteristic and expect to get the recently written
 * information.
 **/
static void ble_central_custom_gatt_operations(const struct peer *peer) {
    const struct peer_dsc *dsc;
    int rc;
    uint8_t value[2];

    dsc = peer_dsc_find_uuid(peer, remote_svc_uuid, remote_chr_uuid,
                             BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
    if (dsc == NULL) {
        MODLOG_DFLT(
            ERROR,
            "Error: Peer lacks a CCCD for the subscribable characteristic\n");
        goto err;
    }

    /*** Write 0x00 and 0x01 (The subscription code) to the CCCD ***/
    value[0] = 1;
    value[1] = 0;
    rc = ble_gattc_write_flat(peer->conn_handle, dsc->dsc.handle, value,
                              sizeof(value), ble_central_on_custom_subscribe,
                              NULL);
    if (rc != 0) {
        MODLOG_DFLT(
            ERROR,
            "Error: Failed to subscribe to the subscribable characteristic; "
            "rc=%d\n",
            rc);
        goto err;
    }

    return;
err:
    /* Terminate the connection */
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Application callback.  Called when the attempt to subscribe to notifications
 * for the ANS Unread Alert Status characteristic has completed.
 */
static int ble_central_on_subscribe(uint16_t conn_handle,
                                    const struct ble_gatt_error *error,
                                    struct ble_gatt_attr *attr, void *arg) {
    struct peer *peer;

    MODLOG_DFLT(INFO,
                "Subscribe complete; status=%d conn_handle=%d "
                "attr_handle=%d\n",
                error->status, conn_handle, attr->handle);

    peer = peer_find(conn_handle);
    if (peer == NULL) {
        MODLOG_DFLT(ERROR, "Error in finding peer, aborting...");
        ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    /* Subscribe to, write to, and read the custom characteristic*/
    ble_central_custom_gatt_operations(peer);

    return 0;
}

/**
 * Application callback.  Called when the write to the ANS Alert Notification
 * Control Point characteristic has completed.
 */
static int ble_central_on_write(uint16_t conn_handle,
                                const struct ble_gatt_error *error,
                                struct ble_gatt_attr *attr, void *arg) {
    MODLOG_DFLT(INFO,
                "Write complete; status=%d conn_handle=%d attr_handle=%d\n",
                error->status, conn_handle, attr->handle);

    /* Subscribe to notifications for the Unread Alert Status characteristic.
     * A central enables notifications by writing two bytes (1, 0) to the
     * characteristic's client-characteristic-configuration-descriptor (CCCD).
     */
    const struct peer_dsc *dsc;
    uint8_t value[2];
    int rc;
    const struct peer *peer = peer_find(conn_handle);

    dsc = peer_dsc_find_uuid(
        peer, BLE_UUID16_DECLARE(BLE_CENTRAL_SVC_ALERT_UUID),
        BLE_UUID16_DECLARE(BLE_CENTRAL_CHR_UNR_ALERT_STAT_UUID),
        BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
    if (dsc == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer lacks a CCCD for the Unread Alert "
                           "Status characteristic\n");
        goto err;
    }

    value[0] = 1;
    value[1] = 0;
    rc = ble_gattc_write_flat(conn_handle, dsc->dsc.handle, value, sizeof value,
                              ble_central_on_subscribe, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR,
                    "Error: Failed to subscribe to characteristic; "
                    "rc=%d\n",
                    rc);
        goto err;
    }

    return 0;
err:
    /* Terminate the connection. */
    return ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Application callback.  Called when the read of the ANS Supported New Alert
 * Category characteristic has completed.
 */
static int ble_central_on_read(uint16_t conn_handle,
                               const struct ble_gatt_error *error,
                               struct ble_gatt_attr *attr, void *arg) {
    MODLOG_DFLT(INFO, "Read complete; status=%d conn_handle=%d", error->status,
                conn_handle);
    if (error->status == 0) {
        MODLOG_DFLT(INFO, " attr_handle=%d value=", attr->handle);
        ble_print_mbuf(attr->om);
    }
    MODLOG_DFLT(INFO, "\n");

    /* Write two bytes (99, 100) to the alert-notification-control-point
     * characteristic.
     */
    const struct peer_chr *chr;
    uint8_t value[2];
    int rc;
    const struct peer *peer = peer_find(conn_handle);

    chr = peer_chr_find_uuid(
        peer, BLE_UUID16_DECLARE(BLE_CENTRAL_SVC_ALERT_UUID),
        BLE_UUID16_DECLARE(BLE_CENTRAL_CHR_ALERT_NOT_CTRL_PT));
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't support the Alert "
                           "Notification Control Point characteristic\n");
        goto err;
    }

    value[0] = 99;
    value[1] = 100;
    rc = ble_gattc_write_flat(conn_handle, chr->chr.val_handle, value,
                              sizeof value, ble_central_on_write, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error: Failed to write characteristic; rc=%d\n",
                    rc);
        goto err;
    }

    return 0;
err:
    /* Terminate the connection. */
    return ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Performs three GATT operations against the specified peer:
 * 1. Reads the ANS Supported New Alert Category characteristic.
 * 2. After read is completed, writes the ANS Alert Notification Control Point
 * characteristic.
 * 3. After write is completed, subscribes to notifications for the ANS Unread
 * Alert Status characteristic.
 *
 * If the peer does not support a required service, characteristic, or
 * descriptor, then the peer lied when it claimed support for the alert
 * notification service!  When this happens, or if a GATT procedure fails,
 * this function immediately terminates the connection.
 */
static void ble_central_read_write_subscribe(const struct peer *peer) {
    const struct peer_chr *chr;
    int rc;

    /* Read the supported-new-alert-category characteristic. */
    chr = peer_chr_find_uuid(
        peer, BLE_UUID16_DECLARE(BLE_CENTRAL_SVC_ALERT_UUID),
        BLE_UUID16_DECLARE(BLE_CENTRAL_CHR_SUP_NEW_ALERT_CAT_UUID));
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't support the Supported New "
                           "Alert Category characteristic\n");
        goto err;
    }

    rc = ble_gattc_read(peer->conn_handle, chr->chr.val_handle,
                        ble_central_on_read, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error: Failed to read characteristic; rc=%d\n", rc);
        goto err;
    }

    return;
err:
    /* Terminate the connection. */
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Called when service discovery of the specified peer has completed.
 */

static void ble_central_on_disc_complete(const struct peer *peer, int status,
                                         void *arg) {

    if (status != 0) {
        /* Service discovery failed.  Terminate the connection. */
        MODLOG_DFLT(ERROR,
                    "Error: Service discovery failed; status=%d "
                    "conn_handle=%d\n",
                    status, peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    /* Service discovery has completed successfully.  Now we have a complete
     * list of services, characteristics, and descriptors that the peer
     * supports.
     */
    MODLOG_DFLT(INFO,
                "Service discovery complete; status=%d "
                "conn_handle=%d\n",
                status, peer->conn_handle);

    /* Now perform three GATT procedures against the peer: read,
     * write, and subscribe to notifications for the ANS service.
     */
    ble_central_read_write_subscribe(peer);
}

static void ble_central_scan(void) {
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params = {0};
    int rc;

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Tell the controller to filter duplicates; we don't want to process
     * repeated advertisements from the same device.
     */
    disc_params.filter_duplicates = 1;

    /**
     * Perform a passive scan.  I.e., don't send follow-up scan requests to
     * each advertiser.
     */
    disc_params.passive = 1;

    /* Use defaults for the rest of the parameters. */
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params,
                      ble_central_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error initiating GAP discovery procedure; rc=%d\n", rc);
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that is
 * established.  ble_central uses the same callback for all connections.
 *
 * @param event                 The event being signalled.
 * @param arg                   Application-specified argument; unused by
 *                                  ble_central.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int ble_central_gap_event(struct ble_gap_event *event, void *arg) {
#if NIMBLE_BLE_CONNECT
    struct ble_gap_conn_desc desc;
#endif
    struct ble_hs_adv_fields fields;
#if MYNEWT_VAL(BLE_HCI_VS)
#if MYNEWT_VAL(BLE_POWER_CONTROL)
    struct ble_gap_set_auto_pcl_params params;
#endif
#endif
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
                                     event->disc.length_data);
        if (rc != 0) {
            return 0;
        }

        /* An advertisement report was received during GAP discovery. */
        ble_print_adv_fields(&fields);

        /* Try to connect to the advertiser if it looks interesting. */
        ble_central_connect_if_interesting(&event->disc);
        return 0;
#if NIMBLE_BLE_CONNECT
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        if (event->connect.status == 0) {
            /* Connection successfully established. */
            MODLOG_DFLT(INFO, "Connection established ");

            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            ble_print_conn_desc(&desc);
            MODLOG_DFLT(INFO, "\n");

            /* Remember peer. */
            rc = peer_add(event->connect.conn_handle);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to add peer; rc=%d\n", rc);
                return 0;
            }

#if MYNEWT_VAL(BLE_POWER_CONTROL)
            ble_central_power_control(event->connect.conn_handle);
#endif

#if MYNEWT_VAL(BLE_HCI_VS)
#if MYNEWT_VAL(BLE_POWER_CONTROL)
            memset(&params, 0x0, sizeof(struct ble_gap_set_auto_pcl_params));
            params.conn_handle = event->connect.conn_handle;
            rc = ble_gap_set_auto_pcl_param(&params);
            if (rc != 0) {
                MODLOG_DFLT(INFO, "Failed to send VSC  %x \n", rc);
                return 0;
            } else {
                MODLOG_DFLT(INFO, "Successfully issued VSC , rc = %d \n", rc);
            }
#endif
#endif

#if CONFIG_UBABEL_BLE_ENCRYPTION
            /** Initiate security - It will perform
             * Pairing (Exchange keys)
             * Bonding (Store keys)
             * Encryption (Enable encryption)
             * Will invoke event BLE_GAP_EVENT_ENC_CHANGE
             **/
            rc = ble_gap_security_initiate(event->connect.conn_handle);
            if (rc != 0) {
                MODLOG_DFLT(INFO, "Security could not be initiated, rc = %d\n",
                            rc);
                return ble_gap_terminate(event->connect.conn_handle,
                                         BLE_ERR_REM_USER_CONN_TERM);
            } else {
                MODLOG_DFLT(INFO, "Connection secured\n");
            }
#else
#if MYNEWT_VAL(BLE_GATTC)
#if MYNEWT_VAL(BLE_GATT_CACHING_ASSOC_ENABLE)
            rc = ble_gattc_cache_assoc(desc.peer_id_addr);
            if (rc != 0) {
                ESP_LOGE(TAG, "Cache Association Failed; rc=%d\n", rc);
                return 0;
            }
#else
            /* Perform service discovery */
            rc = peer_disc_all(event->connect.conn_handle,
                               ble_central_on_disc_complete, NULL);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to discover services; rc=%d\n", rc);
                return 0;
            }
#endif // BLE_GATT_CACHING_ASSOC_ENABLE
#endif // BLE_GATTC
#endif // EXAMPLE_ENCRYPTION
        } else {
            /* Connection attempt failed; resume scanning. */
            ESP_LOGE(TAG, "Error: Connection failed; status=%d\n",
                     event->connect.status);
            ble_central_scan();
        }

        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        /* Connection terminated. */
        MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        ble_print_conn_desc(&event->disconnect.conn);
        MODLOG_DFLT(INFO, "\n");

        /* Forget about peer. */
        peer_delete(event->disconnect.conn.conn_handle);

#if MYNEWT_VAL(BLE_EATT_CHAN_NUM) > 0
        /* Reset EATT config */
        bearers = 0;
        for (int i = 0; i < MYNEWT_VAL(BLE_EATT_CHAN_NUM); i++) {
            cids[i] = 0;
        }
#endif

        /* Resume scanning. */
        ble_central_scan();
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        MODLOG_DFLT(INFO, "discovery complete; reason=%d\n",
                    event->disc_complete.reason);
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        MODLOG_DFLT(INFO, "encryption change event; status=%d ",
                    event->enc_change.status);
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        assert(rc == 0);
        ble_print_conn_desc(&desc);
#if !MYNEWT_VAL(BLE_EATT_CHAN_NUM)
#if CONFIG_UBABEL_BLE_ENCRYPTION && MYNEWT_VAL(BLE_GATTC)
#if MYNEWT_VAL(BLE_GATT_CACHING_ASSOC_ENABLE)
        rc = ble_gattc_cache_assoc(desc.peer_id_addr);
        if (rc != 0) {
            ESP_LOGE(TAG, "Cache Association Failed; rc=%d\n", rc);
            return 0;
        }
#else
        /*** Go for service discovery after encryption has been successfully
         * enabled ***/
        rc = peer_disc_all(event->enc_change.conn_handle,
                           ble_central_on_disc_complete, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to discover services; rc=%d\n", rc);
            return 0;
        }
#endif // BLE_GATT_CACHING_ASSOC_ENABLE
#endif // EXAMPLE_ENCRYPTION
#endif
        return 0;

    case BLE_GAP_EVENT_CACHE_ASSOC:
#if MYNEWT_VAL(BLE_GATT_CACHING_ASSOC_ENABLE)
        /* Cache association result for this connection */
        MODLOG_DFLT(
            INFO,
            "cache association; conn_handle=%d status=%d cache_state=%s\n",
            event->cache_assoc.conn_handle, event->cache_assoc.status,
            (event->cache_assoc.cache_state == 0) ? "INVALID" : "LOADED");
        /* Perform service discovery */
        rc = peer_disc_all(event->connect.conn_handle,
                           ble_central_on_disc_complete, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to discover services; rc=%d\n", rc);
            return 0;
        }
#endif
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        /* Peer sent us a notification or indication. */
        MODLOG_DFLT(INFO,
                    "received %s; conn_handle=%d attr_handle=%d "
                    "attr_len=%d\n",
                    event->notify_rx.indication ? "indication" : "notification",
                    event->notify_rx.conn_handle, event->notify_rx.attr_handle,
                    OS_MBUF_PKTLEN(event->notify_rx.om));

        /* Attribute data is contained in event->notify_rx.om. Use
         * `os_mbuf_copydata` to copy the data received in notification mbuf */
        return 0;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle, event->mtu.channel_id,
                    event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link.
         */

        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;

#if CONFIG_UBABEL_BLE_EXTENDED_ADV
    case BLE_GAP_EVENT_EXT_DISC:
        /* An advertisement report was received during GAP discovery. */
        ble_print_ext_adv_report(&event->ext_disc);

        ble_central_connect_if_interesting(&event->ext_disc);
        return 0;
#endif

#if MYNEWT_VAL(BLE_POWER_CONTROL)
    case BLE_GAP_EVENT_TRANSMIT_POWER:
        MODLOG_DFLT(INFO,
                    "Transmit power event : status=%d conn_handle=%d reason=%d "
                    "phy=%d power_level=%d power_level_flag=%d delta=%d",
                    event->transmit_power.status,
                    event->transmit_power.conn_handle,
                    event->transmit_power.reason, event->transmit_power.phy,
                    event->transmit_power.transmit_power_level,
                    event->transmit_power.transmit_power_level_flag,
                    event->transmit_power.delta);
        return 0;

    case BLE_GAP_EVENT_PATHLOSS_THRESHOLD:
        MODLOG_DFLT(
            INFO,
            "Pathloss threshold event : conn_handle=%d current path loss=%d "
            "zone_entered =%d",
            event->pathloss_threshold.conn_handle,
            event->pathloss_threshold.current_path_loss,
            event->pathloss_threshold.zone_entered);
        return 0;
#endif

#if MYNEWT_VAL(BLE_EATT_CHAN_NUM) > 0
    case BLE_GAP_EVENT_EATT:
        int i;
        MODLOG_DFLT(INFO, "EATT %s : conn_handle=%d cid=%d",
                    event->eatt.status ? "disconnected" : "connected",
                    event->eatt.conn_handle, event->eatt.cid);
        if (event->eatt.status) {
            /* Remove CID from the list of saved CIDs */
            for (i = 0; i < bearers; i++) {
                if (cids[i] == event->eatt.cid) {
                    break;
                }
            }
            while (i < (bearers - 1)) {
                cids[i] = cids[i + 1];
                i += 1;
            }
            cids[i] = 0;

            /* Now Abort */
            return 0;
        }
        cids[bearers] = event->eatt.cid;
        bearers += 1;
        if (bearers != MYNEWT_VAL(BLE_EATT_CHAN_NUM)) {
            /* Wait until all EATT bearers are connected before proceeding */
            return 0;
        }
        /* Set the default bearer to use for further procedures */
        rc = ble_att_set_default_bearer_using_cid(event->eatt.conn_handle,
                                                  cids[0]);
        if (rc != 0) {
            MODLOG_DFLT(INFO, "Cannot set default EATT bearer, rc = %d\n", rc);
            return rc;
        }
#if MYNEWT_VAL(BLE_GATTC)
        /* Perform service discovery */
        rc = peer_disc_all(event->eatt.conn_handle,
                           ble_central_on_disc_complete, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to discover services; rc=%d\n", rc);
            return 0;
        }
#endif
#endif
        return 0;

#endif
    default:
        return 0;
    }
}

void ble_central_on_sync(void) {
    int rc;

    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Begin scanning for a peripheral to connect to. */
    ble_central_scan();
}
