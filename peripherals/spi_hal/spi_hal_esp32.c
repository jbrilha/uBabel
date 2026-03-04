#include <stdio.h>
#include <stdlib.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <unistd.h>

#include "gpio_hal.h"
#include "spi_hal.h"

#ifdef M5STACK_CORE_BASIC
#define MISO_PIN 19
#define SCLK_PIN 18
#define MOSI_PIN 23
#elif defined(CONFIG_IDF_TARGET_ESP32)
#define MISO_PIN 21
#define SCLK_PIN 18
#define MOSI_PIN 19
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
// #define MISO_PIN 40
// #define SCLK_PIN 41
// #define MOSI_PIN 42
// THESE ARE FOR TESTING ON THE SUPERMINI
#define MISO_PIN 1
#define SCLK_PIN 6
#define MOSI_PIN 7
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
#define MISO_PIN 22
#define SCLK_PIN 5
#define MOSI_PIN 6
#elif defined(CONFIG_IDF_TARGET_ESP32C5)
#define MISO_PIN 25
#define SCLK_PIN 23
#define MOSI_PIN 24
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define MISO_PIN 22
#define SCLK_PIN 5
#define MOSI_PIN 6
#endif

static const char *TAG = "SPI_HAL";

static bool spi_initialized = false;
static spi_bus_t spi_host;

bool spi_hal_master_init_bus(spi_bus_t spi_host_bus) {
    spi_bus_config_t config = {
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = MISO_PIN,
        .sclk_io_num = SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,
    };

    esp_err_t err = spi_bus_initialize(spi_host_bus, &config, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(err);

    spi_host = spi_host_bus;
    spi_initialized = true;

    return spi_initialized;
}

bool spi_hal_master_init() { return spi_hal_master_init_bus(SPI2_HOST); }

spi_dev_handle_t spi_hal_add_device_to_bus(spi_bus_t spi_host,
                                           spi_dev_cfg_t *dev_cfg) {
    if (!spi_initialized) {
        LOG_ERROR(TAG, "SPI bus has not been initialized");
        return NULL;
    }

    spi_dev_handle_t spi_device;

    esp_err_t err = spi_bus_add_device(spi_host, dev_cfg, &spi_device);

    if (err != ESP_OK) {
        LOG_ERROR(TAG, "failed to add SPI device %d to bus SPI%d, err: %d",
                  dev_cfg->spics_io_num, spi_host, err);
        return NULL;
    }

    return spi_device;
}

void spi_hal_remove_device(spi_dev_handle_t spi_device) {
    if (!spi_initialized) {
        LOG_ERROR(TAG, "SPI bus has not been initialized");
        return;
    }

    esp_err_t err = spi_bus_remove_device(spi_device);

    if (err != ESP_OK) {
        LOG_ERROR(TAG, "failed to remove SPI device, err: %d", err);
        return;
    }
}

spi_dev_handle_t spi_hal_add_device(spi_dev_cfg_t *dev_cfg) {
    return spi_hal_add_device_to_bus(spi_host, dev_cfg);
}

bool spi_hal_read_register_addr(spi_dev_handle_t device, uint8_t addr,
                               uint8_t *data, size_t len) {
    spi_transaction_t trans = {
        .addr = addr,
        .rxlength = len * 8,
        .length = len * 8,
        .rx_buffer = data,
        .tx_buffer = NULL,
    };

    esp_err_t err = spi_device_transmit(device, &trans);
    return err == ESP_OK;
}

bool spi_hal_read_register_cmd(spi_dev_handle_t device, uint8_t cmd,
                               uint8_t *rx_data, uint8_t *tx_data, size_t len) {

    spi_transaction_t trans = {.cmd = cmd,
                               .rx_buffer = rx_data,
                               .tx_buffer = tx_data,
                               .rxlength = len * 8,
                               .length = len * 8,
                               .flags = 0};
    esp_err_t err = spi_device_polling_transmit(device, &trans);
    return err == ESP_OK;
}

bool spi_hal_write_register_addr(spi_dev_handle_t device, uint8_t addr,
                                 const uint8_t *data, size_t len) {
    spi_transaction_t trans = {.addr = addr,
                               .tx_buffer = data,
                               .rxlength = len * 8,
                               .length = len * 8,
                               .flags = 0};
    esp_err_t err = spi_device_polling_transmit(device, &trans);
    return err == ESP_OK;
}

bool spi_hal_write_register_cmd(spi_dev_handle_t device, uint8_t cmd,
                                const uint8_t *data, size_t len) {
    spi_transaction_t trans = {.cmd = cmd,
                               .tx_buffer = data,
                               .rxlength = len * 8,
                               .length = len * 8,
                               .flags = 0};
    esp_err_t err = spi_device_polling_transmit(device, &trans);
    return err == ESP_OK;
}

spi_dev_cfg_t spi_hal_create_config(int cs_pin, uint32_t clock_speed_hz,
                                    uint8_t mode) {
    spi_dev_cfg_t cfg = {0};

    cfg.spics_io_num = cs_pin;
    cfg.clock_speed_hz = clock_speed_hz;
    cfg.mode = mode;
    cfg.command_bits = 0;
    cfg.address_bits = 8;
    cfg.queue_size = 7;
    cfg.dummy_bits = 0;
    cfg.flags = 0;

    return cfg;
}

bool is_spi_initialized() { return spi_initialized; }
