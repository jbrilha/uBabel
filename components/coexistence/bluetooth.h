#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <esp_err.h>

void ble_mesh_get_dev_uuid(uint8_t *dev_uuid);
esp_err_t bluetooth_init(void);

#endif // !BLUETOOTH_H
