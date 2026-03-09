#ifndef ZB_COORDINATOR_H
#define ZB_COORDINATOR_H

#include "esp_zigbee_core.h"

#define MAX_CHILDREN                    10         /* the max amount of connected devices */
#define HA_ONOFF_SWITCH_ENDPOINT        1          /* esp light switch device endpoint */
#define ZB_C_PRIMARY_CHANNEL_MASK       (1l << 13) /* Zigbee primary channel mask use in the example */

#define ZB_COORDINATOR_CONFIG()                                                \
    {                                                                          \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR,                         \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,                      \
        .nwk_cfg.zczr_cfg =                                                    \
            {                                                                  \
                .max_children = MAX_CHILDREN,                                  \
            },                                                                 \
    }

#endif // !ZB_COORDINATOR_H
