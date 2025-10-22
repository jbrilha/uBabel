#ifndef SD_CARD_H
#define SD_CARD_H

#include "esp_err.h"
#include <stdbool.h>

#define MOUNT_POINT "/sdcard"

#define SDSPI_CS_PIN 1

// these work with the Freenove ESP32-S3-WROOM CAM board
#define SDMMC_CLK_PIN 39
#define SDMMC_CMD_PIN 38
#define SDMMC_D0_PIN 40
// nothing to test these with
#define SDMMC_D1_PIN -1
#define SDMMC_D2_PIN -1
#define SDMMC_D3_PIN -1

esp_err_t mount_sdspi_card(int cs_pin);
esp_err_t mount_sdmmc_card_1w(int clk_pin, int cmd_pin, int d0_pin);
esp_err_t mount_sdmmc_card_4w(int clk_pin, int cmd_pin, int d0_pin, int d1_pin,
                              int d2_pin, int d3_pin);
void unmount_sd_card(void);

esp_err_t write_to_file(const char *file_name, const char *data);
esp_err_t read_file(const char *file_name);
esp_err_t read_dir_contents(const char *dir_name);

bool is_sd_card_mounted(void);

void run_sd_card_task(void);

#endif // !SD_CARD_H
