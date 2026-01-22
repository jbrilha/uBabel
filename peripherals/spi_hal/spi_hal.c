#include <stdio.h>
#include <stdlib.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <unistd.h>

#include "driver/gpio.h"
#include "gpio_hal.h"
#include "spi_hal.h"

#if BUILD_ESP32
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
#endif
#elif BUILD_PICO
#define MISO_PIN PICO_DEFAULT_SPI_RX_PIN  // 16
#define SCLK_PIN PICO_DEFAULT_SPI_SCK_PIN // 18
#define MOSI_PIN PICO_DEFAULT_SPI_TX_PIN  // 19
#endif

static const char *TAG = "SPI_HAL";

static bool spi_initialized = false;
static spi_bus_t spi_host;

bool spi_hal_master_init_bus(spi_bus_t spi_host_bus) {
#if BUILD_ESP32
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
#elif BUILD_PICO
    // SPI0 @ 1MHz
    spi_init(spi_host_bus, 1000 * 1000);
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
#endif

    spi_host = spi_host_bus;
    spi_initialized = true;

    return spi_initialized;
}

bool spi_hal_master_init() {
#if BUILD_ESP32
    return spi_hal_master_init_bus(SPI2_HOST);
#elif BUILD_PICO
    return spi_hal_master_init_bus(spi_default);
#endif
    return false;
}

spi_dev_handle_t spi_hal_add_device_to_bus(spi_bus_t spi_host,
                                           spi_dev_cfg_t *dev_cfg) {
    if (!spi_initialized) {
        LOG_ERROR(TAG, "SPI bus has not been initialized");
        return NULL;
    }

    spi_dev_handle_t spi_device;

#if BUILD_ESP32
    esp_err_t err = spi_bus_add_device(spi_host, dev_cfg, &spi_device);

    if (err != ESP_OK) {
        LOG_ERROR(TAG, "failed to add SPI device %d to bus SPI%d, err: %d",
                  dev_cfg->spics_io_num, spi_host, err);
        return NULL;
    }
#else
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
#endif

    return spi_device;
}

void spi_hal_remove_device(spi_dev_handle_t spi_device) {
    if (!spi_initialized) {
        LOG_ERROR(TAG, "SPI bus has not been initialized");
        return;
    }

#if BUILD_ESP32
    esp_err_t err = spi_bus_remove_device(spi_device);

    if (err != ESP_OK) {
        LOG_ERROR(TAG, "failed to remove SPI device, err: %d", err);
        return;
    }
#else
    // TODO double check this on the pico, haven't tested yet
    free(spi_device);
#endif
}

spi_dev_handle_t spi_hal_add_device(spi_dev_cfg_t *dev_cfg) {
    return spi_hal_add_device_to_bus(spi_host, dev_cfg);
}

bool spi_hal_transmit(spi_dev_handle_t device, uint8_t *data, size_t len) {
#if BUILD_ESP32
    spi_transaction_t trans = {
        .length = len * 8,
        .tx_buffer = data,
    };
    esp_err_t err = spi_device_transmit(device, &trans);
    return err == ESP_OK;
#elif BUILD_PICO
    // need to manually set the bus speed for this particular device, ESP does
    // this automagically in device_transmit
    spi_set_baudrate(device->spi_bus, device->freq_hz);

    // manual CS select (low means slave device is listening)
    gpio_set_pin_level(device->cs_pin, 0);
    int ret = spi_write_blocking(device->spi_bus, data, len);
    gpio_set_pin_level(device->cs_pin, 1);
    return ret == len;
#endif

    return false;
}

bool spi_hal_write_reg_buffer(spi_dev_handle_t device, uint8_t address,
                              const uint8_t *data, size_t len) {
#if BUILD_ESP32
    spi_transaction_t trans = {
        .addr = address,
        .rxlength = len * 8,
        .length = len * 8,
        .rx_buffer = NULL,
        .tx_buffer = data,
    };

    esp_err_t err = spi_device_transmit(device, &trans);
    return err == ESP_OK;
#elif BUILD_PICO
    // need to manually set the bus speed for this particular device, ESP does
    // this automagically in device_transmit
    spi_set_baudrate(device->spi_bus, device->freq_hz);

    // manual CS select (low means slave device is listening)
    gpio_set_pin_level(device->cs_pin, 0);
    int ret = spi_write_blocking(device->spi_bus, data, len);
    gpio_set_pin_level(device->cs_pin, 1);
    return ret == len;
#endif

    return false;
}

bool spi_hal_read_reg_buffer(spi_dev_handle_t device, uint8_t address,
                             uint8_t *data, size_t len) {
#if BUILD_ESP32
    spi_transaction_t trans = {
        .addr = address,
        .rxlength = len * 8,
        .length = len * 8,
        .rx_buffer = data,
        .tx_buffer = NULL,
    };

    esp_err_t err = spi_device_transmit(device, &trans);
    return err == ESP_OK;
#elif BUILD_PICO
    // need to manually set the bus speed for this particular device, ESP does
    // this automagically in device_transmit
    spi_set_baudrate(device->spi_bus, device->freq_hz);

    // manual CS select (low means slave device is listening)
    gpio_set_pin_level(device->cs_pin, 0);
    int ret = spi_write_blocking(device->spi_bus, data, len);
    gpio_set_pin_level(device->cs_pin, 1);
    return ret == len;
#endif

    return false;
}

bool spi_hal_write_register(spi_dev_handle_t device, uint8_t addr,
                            uint8_t *data, size_t len) {
#if BUILD_ESP32
    spi_transaction_t trans = {.addr = addr,
                               .tx_buffer = data,
                               .rxlength = len * 8,
                               .length = len * 8,
                               .flags = 0};
    esp_err_t err = spi_device_polling_transmit(device, &trans);
    return err == ESP_OK;
#elif BUILD_PICO
    LOG_ERROR(TAG, "NOT IMPLEMENTED");
    return false;
#endif

    return false;
}

bool spi_hal_write_register_cmd(spi_dev_handle_t device, uint8_t cmd,
                                uint8_t *data, size_t len) {
#if BUILD_ESP32
    spi_transaction_t trans = {.cmd = cmd,
                               .tx_buffer = data,
                               .rxlength = len * 8,
                               .length = len * 8,
                               .flags = 0};
    esp_err_t err = spi_device_polling_transmit(device, &trans);
    return err == ESP_OK;
#elif BUILD_PICO
    LOG_ERROR(TAG, "NOT IMPLEMENTED");
    return false;
#endif

    return false;
}

bool spi_hal_write_register_cmd_addr(spi_dev_handle_t device, uint8_t cmd,
                                     uint16_t addr, uint8_t *data, size_t len) {
#if BUILD_ESP32
    spi_transaction_t trans = {.cmd = cmd,
                               .addr = addr,
                               .tx_buffer = data,
                               .rxlength = len * 8,
                               .length = len * 8,
                               .flags = 0};
    esp_err_t err = spi_device_polling_transmit(device, &trans);
    return err == ESP_OK;
#elif BUILD_PICO
    LOG_ERROR(TAG, "NOT IMPLEMENTED");
    return false;
#endif

    return false;
}

bool spi_hal_read_register_cmd_addr(spi_dev_handle_t device, uint8_t cmd,
                                    uint16_t addr, uint8_t *rx_data,
                                    uint8_t *tx_data, size_t len) {

#if BUILD_ESP32
    spi_transaction_t trans = {.cmd = cmd,
                               .addr = addr,
                               .rx_buffer = rx_data,
                               .tx_buffer = tx_data,
                               .rxlength = len * 8,
                               .length = len * 8,
                               .flags = 0};
    esp_err_t err = spi_device_polling_transmit(device, &trans);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "Failed to do polling transmit");
        return false;
    }
    return true;
#elif BUILD_PICO
    LOG_ERROR(TAG, "NOT IMPLEMENTED");
    return false;
#endif

    return false;
}

// TODO rename this
bool spi_hal_read_register_cmd(spi_dev_handle_t device, uint8_t cmd,
                               uint8_t *rx_data, uint8_t *tx_data, size_t len) {

#if BUILD_ESP32
    spi_transaction_t trans = {.cmd = cmd,
                               .rx_buffer = rx_data,
                               .tx_buffer = tx_data,
                               .rxlength = len * 8,
                               .length = len * 8,
                               .flags = 0};
    esp_err_t err = spi_device_polling_transmit(device, &trans);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "Failed to do polling transmit");
        return false;
    }
    return true;
#elif BUILD_PICO
    LOG_ERROR(TAG, "NOT IMPLEMENTED");
    return false;
#endif

    return false;
}

bool spi_hal_read_register(spi_dev_handle_t device, uint8_t addr, uint8_t *data,
                           size_t len) {

#if BUILD_ESP32
    *data = 0;
    spi_transaction_t trans = {.addr = addr,
                               .rx_buffer = NULL,
                               .tx_buffer = NULL,
                               .rxlength = len * 8,
                               .length = len * 8,
                               .flags = SPI_TRANS_USE_RXDATA};
    esp_err_t err = spi_device_polling_transmit(device, &trans);
    if (err != ESP_OK) {
        LOG_ERROR(TAG, "Failed to do polling transmit");
        return false;
    }
    for (int i = 0; i < len; i++) {
        *data = ((*data) << 8);
        *data = (*data) + trans.rx_data[i];
    }
    return true;
#elif BUILD_PICO
    LOG_ERROR(TAG, "NOT IMPLEMENTED");
    return false;
#endif

    return false;
}

bool spi_hal_receive_w_dummy(spi_dev_handle_t device, uint8_t *data, size_t len,
                             uint8_t dummy_byte) {
#if BUILD_ESP32
    spi_transaction_t trans = {
        .length = len * 8,
        .rx_buffer = data,
    };

    if (dummy_byte == 0x00) {
        trans.tx_buffer = NULL; // if it's 0x00 no need to do allocations
    } else {
        // must provide a buffer because for each MOSI byte, there needs to be a
        // MISO byte
        uint8_t *dummy_buf = malloc(len);
        memset(dummy_buf, dummy_byte, len);
        trans.tx_buffer = dummy_buf;

        esp_err_t err = spi_device_transmit(device, &trans);
        free(dummy_buf);
        return err == ESP_OK;
    }

    esp_err_t err = spi_device_transmit(device, &trans);
    return err == ESP_OK;
#elif BUILD_PICO
    spi_set_baudrate(device->spi_bus, device->freq_hz);
    gpio_put(device->cs_pin, 0);
    int ret = spi_read_blocking(device->spi_bus, dummy_byte, data, len);
    gpio_put(device->cs_pin, 1);
    return ret == len;
#endif
}

bool spi_hal_receive(spi_dev_handle_t device, uint8_t *data, size_t len) {
    // 0x00 is the default dummy byte on ESP-IDF when tx_buffer is NULL, and
    // generally can be 0 unless a specific one is needed (SD cards  expect
    // 0xFF, per the pico docs)
    return spi_hal_receive_w_dummy(device, data, len, 0x00);
}

// full duplex single byte transfer like arudino's SPI.transfer()
uint8_t spi_hal_transfer(spi_dev_handle_t device, uint8_t tx_byte) {
    uint8_t rx_byte;
#if BUILD_ESP32
    spi_transaction_t trans = {
        .length = 8,
        .tx_buffer = &tx_byte,
        .rx_buffer = &rx_byte,
    };
    spi_device_transmit(device, &trans);
#elif BUILD_PICO
    spi_set_baudrate(device->spi_bus, device->freq_hz);

    gpio_set_pin_level(device->cs_pin, 0);
    spi_write_read_blocking(device->spi_bus, &tx_byte, &rx_byte, 1);
    gpio_set_pin_level(device->cs_pin, 1);
#endif

    return rx_byte;
}

bool spi_hal_write_then_read(spi_dev_handle_t device, uint8_t *tx_data,
                             size_t tx_len, uint8_t *rx_data, size_t rx_len,
                             uint8_t dummy_byte) {
#if BUILD_ESP32
    // to keep it locked between write and read
    spi_device_acquire_bus(device, portMAX_DELAY);

    spi_transaction_t tx_trans = {.length = tx_len * 8,
                                  .tx_buffer = tx_data,
                                  .flags = SPI_TRANS_CS_KEEP_ACTIVE};
    esp_err_t err = spi_device_transmit(device, &tx_trans);
    if (err != ESP_OK) {
        spi_device_release_bus(device);
        return false;
    }

    spi_transaction_t rx_trans = {
        .length = rx_len * 8,
        .rx_buffer = rx_data,
    };

    if (dummy_byte == 0x00) {
        rx_trans.tx_buffer = NULL;
    } else {
        uint8_t *dummy_buf = malloc(rx_len);
        memset(dummy_buf, dummy_byte, rx_len);
        rx_trans.tx_buffer = dummy_buf;

        err = spi_device_transmit(device, &rx_trans);
        free(dummy_buf);
        spi_device_release_bus(device);
        return err == ESP_OK;
    }

    err = spi_device_transmit(device, &rx_trans);
    spi_device_release_bus(device);
    return err == ESP_OK;

#elif BUILD_PICO
    spi_set_baudrate(device->spi_bus, device->freq_hz);

    gpio_set_pin_level(device->cs_pin, 0);

    int ret = spi_write_blocking(device->spi_bus, tx_data, tx_len);
    if (ret != tx_len) {
        gpio_set_pin_level(device->cs_pin, 1);
        return false;
    }

    ret = spi_read_blocking(device->spi_bus, dummy_byte, rx_data, rx_len);

    gpio_set_pin_level(device->cs_pin, 1);
    return ret == rx_len;
#endif
}

spi_dev_cfg_t spi_hal_create_config(int cs_pin, uint32_t clock_speed_hz,
                                    uint8_t mode) {
    spi_dev_cfg_t cfg = {0};

    cfg.spics_io_num = cs_pin;
    cfg.clock_speed_hz = clock_speed_hz;
    cfg.mode = mode;

#if BUILD_ESP32
    cfg.queue_size = 7;
    cfg.command_bits = 0;
    cfg.address_bits = 8;
    cfg.dummy_bits = 0;
    cfg.flags = 0;
#endif

    return cfg;
}

bool is_spi_initialized() { return spi_initialized; }
