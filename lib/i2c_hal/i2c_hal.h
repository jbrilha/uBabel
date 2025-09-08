#ifndef I2C_HAL_H
#define I2C_HAL_H

#include <stdbool.h>

bool i2c_init_default(void);
bool i2c_init_on_pins(int sda, int scl);

#ifdef BUILD_PICO
void i2c_scan_task(void *params);
#endif

#endif // !I2C_HAL_H
