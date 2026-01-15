#include "i2c_hal.h"

#include "platform.h"
#include <string.h>

#if BUILD_ESP32

#define I2C_FREQ_HZ (200 * 1000)

#if CONFIG_IDF_TARGET_ESP32
#define SDA_PIN 21
#define SCL_PIN 22
#else // the C6 has these specific pins but the S3 can use whichever ones
#define SDA_PIN 8
#define SCL_PIN 7
#endif

#define I2C_SCAN_TIMEOUT_MS (50)
#define I2C_MASTER_TIMEOUT_MS (500)
static i2c_master_bus_handle_t bus_handle = NULL;

#elif BUILD_PICO
#include "hardware/i2c.h"

#define I2C_FREQ_HZ (100 * 1000)

// using the defaults does not work with the Grove hat!!
#define SDA_PIN 8
#define SCL_PIN 9

#endif

const static char *TAG = "I2C_HAL";

static bool i2c_initialized = false;

bool i2c_init_on_pins(int sda, int scl) {
#if BUILD_ESP32
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));
#else
    i2c_init(i2c_default, I2C_FREQ_HZ);

    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_set_function(scl, GPIO_FUNC_I2C);

    gpio_pull_up(sda);
    gpio_pull_up(scl);
#endif
    return true;
}

bool i2c_init_default(void) { return i2c_init_on_pins(SDA_PIN, SCL_PIN); }

bool i2c_create_device(i2c_device_handle_t *device, uint16_t addr) {
#if BUILD_ESP32
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = I2C_FREQ_HZ,
    };

    device->addr = addr;

    esp_err_t err =
        i2c_master_bus_add_device(bus_handle, &dev_config, &device->esp_handle);

    return err == ESP_OK;
#else
    device->addr = addr;
    return true; // pico doesn't do handles, so....
#endif
}

bool i2c_write_bytes(i2c_device_handle_t *device, uint8_t *data, size_t len) {
#if BUILD_ESP32
    esp_err_t err = i2c_master_transmit(device->esp_handle, data, len,
                                        I2C_MASTER_TIMEOUT_MS);
    return err == ESP_OK;
#else
    int ret = i2c_write_blocking(i2c_default, device->addr, data, len, false);
    return ret >= 0;
#endif
}

bool i2c_write_byte(i2c_device_handle_t *device, uint8_t data) {
#if BUILD_ESP32
    esp_err_t err = i2c_master_transmit(device->esp_handle, &data, 1,
                                        I2C_MASTER_TIMEOUT_MS);
    return err == ESP_OK;
#else
    int ret = i2c_write_blocking(i2c_default, device->addr, &data, 1, false);
    return ret >= 0;
#endif
}

bool i2c_read_bytes(i2c_device_handle_t *device, uint8_t *data, size_t len) {
#if BUILD_ESP32
    esp_err_t err = i2c_master_receive(device->esp_handle, data, len,
                                       I2C_MASTER_TIMEOUT_MS);

    return err == ESP_OK;
#else
    int ret = i2c_read_blocking(i2c_default, device->addr, data, len, false);

    return ret >= 0;
#endif
}

bool i2c_read_byte(i2c_device_handle_t *device, uint8_t *data) {
#if BUILD_ESP32
    esp_err_t err =
        i2c_master_receive(device->esp_handle, data, 1, I2C_MASTER_TIMEOUT_MS);

    return err == ESP_OK;
#else
    int ret = i2c_read_blocking(i2c_default, device->addr, data, 1, false);

    return ret >= 0;
#endif
}

bool i2c_write_register(i2c_device_handle_t *device, uint8_t reg_addr,
                        uint8_t *data, size_t len) {
    uint8_t tx_buf[len + 1];
    tx_buf[0] = reg_addr;
    memcpy(&tx_buf[1], data, len);

#if BUILD_ESP32
    esp_err_t err = i2c_master_transmit(device->esp_handle, tx_buf, len + 1,
                                        I2C_MASTER_TIMEOUT_MS);

    return err == ESP_OK;
#else
    int ret =
        i2c_write_blocking(i2c_default, device->addr, tx_buf, len + 1, false);

    return ret >= 0;
#endif
}

bool i2c_read_register(i2c_device_handle_t *device, uint8_t reg_addr,
                       uint8_t *data, size_t len) {
#if BUILD_ESP32
    esp_err_t err = i2c_master_transmit_receive(
        device->esp_handle, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS);
    return err == ESP_OK;
#else
    int ret = i2c_write_blocking(i2c_default, device->addr, &reg_addr, 1, true);
    if (ret < 0)
        return false;

    ret = i2c_read_blocking(i2c_default, device->addr, data, len, false);

    return ret >= 0;
#endif
}

static bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

#if BUILD_ESP32
void i2c_scan_loop() {

    uint8_t address;
    while (true) {
        printf("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\r\n");
        for (int i = 0; i < 128; i += 16) {
            printf("%02x: ", i);
            for (int j = 0; j < 16; j++) {
                fflush(stdout);
                address = i + j;
                esp_err_t ret;
                if (reserved_addr(address)) {
                    ret = ESP_ERR_INVALID_ARG;
                } else {
                    ret = i2c_master_probe(bus_handle, address,
                                           I2C_SCAN_TIMEOUT_MS);
                }

                if (ret == ESP_OK) {
                    printf("%02x ", address);
                } else if (ret == ESP_ERR_TIMEOUT) {
                    printf("UU ");
                } else {
                    printf("-- ");
                }
            }
            printf("\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

#elif BUILD_PICO

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
#endif

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
