#ifndef ZIGBEE_H
#define ZIGBEE_H

#include "esp_zigbee_core.h"

#define INSTALLCODE_POLICY_ENABLE       false      /* enable the install code policy for security */

#define ESP_MANUFACTURER_NAME "\x09""ESPRESSIF"      /* Customized manufacturer name */
#define ESP_MODEL_IDENTIFIER "\x07"CONFIG_IDF_TARGET /* Customized model identifier */

#define ZB_DEFAULT_RADIO_CONFIG()                                              \
    {                                                                          \
        .radio_mode = ZB_RADIO_MODE_NATIVE,                                    \
    }

#define ZB_DEFAULT_HOST_CONFIG()                                               \
    {                                                                          \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,                  \
    }

bool zigbee_platform_init(void);
void zigbee_start(void);

#endif // !ZIGBEE_H
