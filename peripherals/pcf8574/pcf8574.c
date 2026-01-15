#include "pcf8574.h"
#include "i2c_hal.h"
#include "platform.h"

static bool pcd_initialized = false;
static i2c_device_handle_t dev_handle;

uint8_t d_in = 0;
uint8_t d_out = 0xFF;

bool PCF8574_init(void) {
    if (pcd_initialized)
        return true;

    if (!i2c_create_device(&dev_handle, PCF8574_ADDR)) {
        LOG_ERROR("PCF8574", "failed to create device");
        return false;
    }

    if (!PCF8574_write_8(0xFF)) {
        LOG_ERROR("PCF8574", "failed to write8");
        return false;
    }

    pcd_initialized = true;
    return true;
}

uint8_t PCF8574_read_8() {
    i2c_read_bytes(&dev_handle, &d_in, 1);
    return d_in;
}

bool PCF8574_read(const uint8_t pin) {
    if (pin > 7) {
        return false;
    }
    PCF8574_read_8();
    return (d_in & (1 << pin)) > 0;
}

bool PCF8574_write_8(uint8_t value) {
    return i2c_write_bytes(&dev_handle, &value, 1);
}

bool PCF8574_write(const uint8_t pin, const uint8_t value) {
    if (pin > 7) {
        return false;
    }

    if (value) {
        d_out |= (1 << pin);
    } else {
        d_out &= ~(1 << pin);
    }

    return PCF8574_write_8(d_out);
}
