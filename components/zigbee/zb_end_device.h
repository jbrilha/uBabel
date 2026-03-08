#ifndef ZB_END_DEVICE_H
#define ZB_END_DEVICE_H

#include "esp_zigbee_core.h"

void zb_ed_task(void* params);

#define ED_AGING_TIMEOUT                ESP_ZB_ED_AGING_TIMEOUT_64MIN        /* aging timeout of device */
#define ED_KEEP_ALIVE                   3000                                 /* 3000 millisecond */
#define HA_ESP_LIGHT_ENDPOINT           10                                   /* esp light bulb device endpoint, used to process light controlling commands */
#define ZB_ED_PRIMARY_CHANNEL_MASK      ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK /* Zigbee primary channel mask use in the example */

#define ZB_END_DEV_CONFIG()                                                    \
    {                                                                          \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,                                  \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,                      \
        .nwk_cfg.zed_cfg =                                                     \
            {                                                                  \
                .ed_timeout = ED_AGING_TIMEOUT,                                \
                .keep_alive = ED_KEEP_ALIVE,                                   \
            },                                                                 \
    }

#endif // !ZB_END_DEVICE_H
