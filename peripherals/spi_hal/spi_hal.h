#ifndef SPI_HAL_H
#define SPI_HAL_H

#include "platform.h"

#if BUILD_ESP32
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "hal/spi_types.h"

typedef spi_host_device_t spi_bus_t;
typedef spi_device_handle_t spi_dev_handle_t;
typedef spi_device_interface_config_t spi_dev_cfg_t;

#elif BUILD_PICO
#include "hardware/spi.h"
#include "pico/stdlib.h"

typedef spi_inst_t *spi_bus_t;

typedef struct {
    spi_bus_t spi_bus;
    uint cs_pin;
    uint freq_hz;
} pico_spi_dev_t;

typedef pico_spi_dev_t *spi_dev_handle_t;

typedef struct {
    uint spics_io_num;
    uint clock_speed_hz;
    uint8_t mode;
    uint8_t command_bits;
    uint8_t address_bits;
} pico_spi_dev_cfg_t;

typedef pico_spi_dev_cfg_t spi_dev_cfg_t;

#endif

bool spi_hal_master_init_bus(spi_bus_t spi_host_bus);
bool spi_hal_master_init();

spi_dev_handle_t spi_hal_add_device_to_bus(spi_bus_t spi_host,
                                           spi_dev_cfg_t *dev_cfg);
spi_dev_handle_t spi_hal_add_device(spi_dev_cfg_t *dev_cfg);
void spi_hal_remove_device(spi_dev_handle_t spi_device);

bool spi_hal_read_register_addr(spi_dev_handle_t device, uint8_t addr,
                                uint8_t *data, size_t len);
bool spi_hal_read_register_cmd(spi_dev_handle_t device, uint8_t cmd,
                               uint8_t *rx_data, uint8_t *tx_data, size_t len);

bool spi_hal_write_register_addr(spi_dev_handle_t device, uint8_t addr,
                                 const uint8_t *data, size_t len);
bool spi_hal_write_register_cmd(spi_dev_handle_t device, uint8_t cmd,
                                const uint8_t *data, size_t len);

uint8_t spi_hal_transfer(spi_dev_handle_t device, uint8_t tx_byte);

spi_dev_cfg_t spi_hal_create_config(int cs_pin, uint32_t clock_speed_hz,
                                    uint8_t mode);

bool is_spi_initialized();

#endif // !SPI_HAL_H
