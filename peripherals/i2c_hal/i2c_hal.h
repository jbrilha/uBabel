#ifndef I2C_HAL_H
#define I2C_HAL_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#if BUILD_ESP32
#include "driver/i2c_master.h"
#endif

typedef struct {
    uint16_t addr;
#if BUILD_ESP32
    i2c_master_dev_handle_t esp_handle;
#endif
} i2c_device_handle_t;

bool i2c_init_default(void);
bool i2c_init_on_pins(int sda, int scl);

bool i2c_create_device(i2c_device_handle_t *device, uint16_t addr);

bool i2c_write_bytes(i2c_device_handle_t *device, uint8_t *data, size_t len);
bool i2c_read_bytes(i2c_device_handle_t *device, uint8_t *data, size_t len);

bool i2c_write_register(i2c_device_handle_t *device, uint8_t reg_addr,
                        uint8_t *data, size_t len);
bool i2c_read_register(i2c_device_handle_t *device, uint8_t reg_addr,
                       uint8_t *data, size_t len);

void i2c_run_scan(void);

#endif // !I2C_HAL_H
