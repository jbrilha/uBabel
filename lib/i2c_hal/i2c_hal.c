#include "i2c_hal.h"

#include "platform.h"

#if BUILD_ESP32
#include "driver/i2c_master.h"

#define I2C_FREQ_HZ (400 * 1000)

#if CONFIG_IDF_TARGET_ESP32
#define SDA_PIN 21
#define SCL_PIN 22
#else // the C6 has these specific pins but the S3 can use whichever ones
#define SDA_PIN 6
#define SCL_PIN 7
#endif

static i2c_master_bus_handle_t bus_handle = NULL;

#elif BUILD_PICO
#include "hardware/i2c.h"

#define I2C_FREQ_HZ (100 * 1000)

// using the defaults does not work with the Grove hat!!
#define SDA_PIN 8
#define SCL_PIN 9

#endif

const static char *TAG = "ADC_HAL";

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

bool i2c_write_register(void) {
#if BUILD_ESP32
#else
#endif
    return false;
}

bool i2c_read_register(void) {
#if BUILD_ESP32
#else
#endif
    return false;
}

#ifdef BUILD_PICO
bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

// from the pico-examples repo
void i2c_scan_task(void *params) {
    i2c_init_default();

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

    vTaskDelete(NULL);
}
#endif
