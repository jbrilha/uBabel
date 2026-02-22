#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <unistd.h>

#include "gpio_hal.h"
#include "spi_hal.h"

#define MISO_PIN PICO_DEFAULT_SPI_RX_PIN  // 16
#define SCLK_PIN PICO_DEFAULT_SPI_SCK_PIN // 18
#define MOSI_PIN PICO_DEFAULT_SPI_TX_PIN  // 19

static const char *TAG = "SPI_HAL";

static bool spi_initialized = false;
static spi_bus_t spi_host;

bool spi_hal_master_init_bus(spi_bus_t spi_host_bus) {
    // SPI0 @ 1MHz
    spi_init(spi_host_bus, 1000 * 1000);
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

    spi_host = spi_host_bus;
    spi_initialized = true;

    return spi_initialized;
}

bool spi_hal_master_init() { return spi_hal_master_init_bus(spi_default); }

spi_dev_handle_t spi_hal_add_device_to_bus(spi_bus_t spi_host,
                                           spi_dev_cfg_t *dev_cfg) {
    if (!spi_initialized) {
        LOG_ERROR(TAG, "SPI bus has not been initialized");
        return NULL;
    }

    spi_dev_handle_t spi_device;

    spi_device = (spi_dev_handle_t)malloc(sizeof(pico_spi_dev_t));
    if (!spi_device)
        return NULL;

    spi_device->spi_bus = spi_host;
    spi_device->cs_pin = dev_cfg->spics_io_num;
    spi_device->freq_hz = dev_cfg->clock_speed_hz;

    // this stuff is handled internally on the ESP side of things
    gpio_init_pin(spi_device->cs_pin);
    gpio_set_pin_dir(spi_device->cs_pin, GPIO_OUTPUT);
    gpio_set_pin_level(spi_device->cs_pin, 1);

    return spi_device;
}

void spi_hal_remove_device(spi_dev_handle_t spi_device) {
    if (!spi_initialized) {
        LOG_ERROR(TAG, "SPI bus has not been initialized");
        return;
    }

    // TODO double check this on the pico, haven't tested yet
    free(spi_device);
}

spi_dev_handle_t spi_hal_add_device(spi_dev_cfg_t *dev_cfg) {
    return spi_hal_add_device_to_bus(spi_host, dev_cfg);
}

bool spi_hal_read_register_addr(spi_dev_handle_t device, uint8_t addr,
                                uint8_t *data, size_t len) {
    uint8_t tx_buf[1 + len];
    tx_buf[0] = addr;
    memset(&tx_buf[1], 0x00, len);

    // need to manually set the bus speed for this particular device, ESP does
    // this automagically in device_transmit
    spi_set_baudrate(device->spi_bus, device->freq_hz);

    // manual CS select (low means slave device is listening)
    gpio_set_pin_level(device->cs_pin, 0);
    int ret = spi_write_read_blocking(device->spi_bus, tx_buf, data, len + 1);
    gpio_set_pin_level(device->cs_pin, 1);
    return ret == len;
}

bool spi_hal_read_register_cmd(spi_dev_handle_t device, uint8_t cmd,
                               uint8_t *rx_data, uint8_t *tx_data, size_t len) {

    uint8_t tx_buf[1 + len];
    tx_buf[0] = cmd;
    memcpy(&tx_buf[1], tx_data, len);

    // need to manually set the bus speed for this particular device, ESP does
    // this automagically in device_transmit
    spi_set_baudrate(device->spi_bus, device->freq_hz);

    // manual CS select (low means slave device is listening)
    gpio_set_pin_level(device->cs_pin, 0);
    int ret = spi_write_read_blocking(device->spi_bus, tx_buf, rx_data, len + 1);
    gpio_set_pin_level(device->cs_pin, 1);
    return ret == len;
}

bool spi_hal_write_register_addr(spi_dev_handle_t device, uint8_t addr,
                                 const uint8_t *data, size_t len) {
    uint8_t buf[1 + len];
    buf[0] = addr;
    memcpy(&buf[1], data, len);

    // need to manually set the bus speed for this particular device, ESP does
    // this automagically in device_transmit
    spi_set_baudrate(device->spi_bus, device->freq_hz);

    // manual CS select (low means slave device is listening)
    gpio_set_pin_level(device->cs_pin, 0);
    int ret = spi_write_blocking(device->spi_bus, data, len + 1);
    gpio_set_pin_level(device->cs_pin, 1);
    return ret == len;
}

bool spi_hal_write_register_cmd(spi_dev_handle_t device, uint8_t cmd,
                                const uint8_t *data, size_t len) {

    uint8_t buf[1 + len];
    buf[0] = cmd;
    memcpy(&buf[1], data, len);

    spi_set_baudrate(device->spi_bus, device->freq_hz);

    // manual CS select (low means slave device is listening)
    gpio_set_pin_level(device->cs_pin, 0);
    int ret = spi_write_blocking(device->spi_bus, data, len + 1);
    gpio_set_pin_level(device->cs_pin, 1);
    return false;
}

spi_dev_cfg_t spi_hal_create_config(int cs_pin, uint32_t clock_speed_hz,
                                    uint8_t mode) {
    spi_dev_cfg_t cfg = {0};

    cfg.spics_io_num = cs_pin;
    cfg.clock_speed_hz = clock_speed_hz;
    cfg.mode = mode;
    cfg.command_bits = 0;
    cfg.address_bits = 8;

    return cfg;
}

bool is_spi_initialized() { return spi_initialized; }
