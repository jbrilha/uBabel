#ifndef SD_CARD_H
#define SD_CARD_H

#include "esp_err.h"
#include <stdbool.h>

esp_err_t mount_sd_card(void);
void unmount_sd_card(void);

esp_err_t write_to_file(const char *file_name, const char *data);
esp_err_t read_file(const char *file_name);
esp_err_t read_dir_contents(const char *dir_name);

bool is_sd_card_mounted(void);

void run_sd_card_task(void);

#endif // !SD_CARD_H
