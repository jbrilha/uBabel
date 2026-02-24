#include "i2c_hal.h"

#include "platform.h"
#include <string.h>

#include "hardware/i2c.h"

#define I2C_FREQ_HZ (100 * 1000)

// using the defaults does not work with the Grove hat!!
#define SDA_PIN 8
#define SCL_PIN 9

const static char *TAG = "I2C_HAL";

static bool i2c_initialized = false;

bool i2c_init_on_pins(int sda, int scl) {
    i2c_init(i2c_default, I2C_FREQ_HZ);

    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_set_function(scl, GPIO_FUNC_I2C);

    gpio_pull_up(sda);
    gpio_pull_up(scl);
    return true;
}

bool i2c_init_default(void) { return i2c_init_on_pins(SDA_PIN, SCL_PIN); }

bool i2c_create_device(i2c_device_handle_t *device, uint16_t addr) {
    device->addr = addr;
    return true; // pico doesn't do handles, so....
}

bool i2c_write_bytes(i2c_device_handle_t *device, uint8_t *data, size_t len) {
    int ret = i2c_write_blocking(i2c_default, device->addr, data, len, false);
    return ret >= 0;
}

bool i2c_write_byte(i2c_device_handle_t *device, uint8_t data) {
    int ret = i2c_write_blocking(i2c_default, device->addr, &data, 1, false);
    return ret >= 0;
}

bool i2c_read_bytes(i2c_device_handle_t *device, uint8_t *data, size_t len) {
    int ret = i2c_read_blocking(i2c_default, device->addr, data, len, false);

    return ret >= 0;
}

bool i2c_read_byte(i2c_device_handle_t *device, uint8_t *data) {
    int ret = i2c_read_blocking(i2c_default, device->addr, data, 1, false);

    return ret >= 0;
}

bool i2c_write_register(i2c_device_handle_t *device, uint8_t reg_addr,
                        uint8_t *data, size_t len) {
    uint8_t tx_buf[len + 1];
    tx_buf[0] = reg_addr;
    memcpy(&tx_buf[1], data, len);

    int ret =
        i2c_write_blocking(i2c_default, device->addr, tx_buf, len + 1, false);

    return ret >= 0;
}

bool i2c_read_register(i2c_device_handle_t *device, uint8_t reg_addr,
                       uint8_t *data, size_t len) {
    int ret = i2c_write_blocking(i2c_default, device->addr, &reg_addr, 1, true);
    if (ret < 0)
        return false;

    ret = i2c_read_blocking(i2c_default, device->addr, data, len, false);

    return ret >= 0;
}

static bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}


// from the pico-examples repo
void i2c_scan_loop() {
    while (true) {
        printf("\nI2C Bus Scan\n");
        printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

        for (int addr = 0; addr < (1 << 7); ++addr) {
            if (addr % 16 == 0) {
                printf("%02x ", addr);
            }

            // Perform a 1-byte dummy read from the probe address. If a slave
            // acknowledges this address, the function returns the number of
            // bytes transferred. If the address byte is ignored, the function
            // returns -1.

            // Skip over any reserved addresses.
            int ret;
            uint8_t rxdata;
            if (reserved_addr(addr))
                ret = PICO_ERROR_GENERIC;
            else
                ret = i2c_read_blocking(i2c_default, addr, &rxdata, 1, false);

            printf(ret < 0 ? "." : "@");
            printf(addr % 16 == 15 ? "\n" : "  ");
        }
        printf("Done.\n");

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void i2c_scan_task(void *params) {
    i2c_scan_loop();

    vTaskDelete(NULL);
}

void i2c_run_scan(void) {
    xTaskCreate(i2c_scan_task,   // Task function
                "i2c_scan_task", // Task name
                4096,            // Stack size in words
                NULL,            // Task parameters
                1,               // Priority
                NULL             // Task handle
    );
}
