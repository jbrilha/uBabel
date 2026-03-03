#ifndef BT_HOST_H
#define BT_HOST_H

#include <esp_err.h>

esp_err_t bt_host_init_as_central(void);
esp_err_t bt_host_init_as_peripheral(void);

#endif // !BT_HOST_H
