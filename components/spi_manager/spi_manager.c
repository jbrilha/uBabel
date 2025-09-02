#include <stdio.h>
#include <sys/lock.h>
#include <sys/param.h>
#include <unistd.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "hal/spi_types.h"

static const char *TAG = "SPI_MANAGER";

#ifdef M5STACK_CORE_BASIC
#define MISO_PIN 19
#define SCLK_PIN 18
#define MOSI_PIN 23
#elif defined(CONFIG_IDF_TARGET_ESP32)
#define MISO_PIN 21
#define SCLK_PIN 18
#define MOSI_PIN 19
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define MISO_PIN 13
#define SCLK_PIN 12
#define MOSI_PIN 11
#endif

static bool spi_initialized = false;
static spi_host_device_t spi_host;

bool spi_manager_init_bus(spi_host_device_t spi_host_bus) {
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

bool spi_manager_init() { return spi_manager_init_bus(SPI2_HOST); }

spi_device_handle_t
spi_manager_add_device_to_bus(spi_host_device_t spi_host,
                              spi_device_interface_config_t *dev_cfg) {
    if (!spi_initialized) {
        ESP_LOGE(TAG, "SPI bus has not been initialized");
        return NULL;
    }

    spi_device_handle_t spi_device;

    esp_err_t err = spi_bus_add_device(spi_host, dev_cfg, &spi_device);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to add SPI device %d to bus SPI%d, err: %d",
                 dev_cfg->spics_io_num, spi_host, err);
        return NULL;
    }
    return spi_device;
}

spi_device_handle_t
spi_manager_add_device(spi_device_interface_config_t *dev_cfg) {
    return spi_manager_add_device_to_bus(spi_host, dev_cfg);
}

bool is_spi_initialized() { return spi_initialized; }
