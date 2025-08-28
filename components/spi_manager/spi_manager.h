#ifndef SPI_MANAGER_H
#define SPI_MANAGER_H

#include "driver/spi_master.h"
#include "platform.h"

spi_device_handle_t
spi_manager_add_device(spi_device_interface_config_t *dev_cfg);

bool spi_manager_init();

#endif // !SPI_MANAGER_H
