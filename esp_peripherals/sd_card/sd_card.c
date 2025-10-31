/* SD card and FAT filesystem example.
   This example uses SPI peripheral to communicate with SD card.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

// https://github.com/espressif/esp-idf/tree/master/examples/storage/sd_card/sdspi

#include "driver/sdmmc_host.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/projdefs.h"
#include "sdmmc_cmd.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <unistd.h>

#include "sd_card.h"

#include "jpeg_decoder.h"
#include "lvgl_camera_widget.h"

#define MAX_CHAR_SIZE 64
#define MAX_PATH_SIZE 64

static const char *TAG = "SD Card";

static bool sd_card_mounted = false;
static sdmmc_card_t *card = NULL;

bool is_sd_card_mounted(void) { return sd_card_mounted; }

static esp_vfs_fat_sdmmc_mount_config_t init_mount_config(void) {
    // if format_if_mount_failed is set to true, the SD card will be partitioned
    // and formatted in case mounting fails, probably not recommended :P
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    return mount_config;
}

static esp_err_t
mount_sdmmc_card(esp_vfs_fat_sdmmc_mount_config_t *mount_config,
                 sdmmc_host_t *host, sdmmc_slot_config_t *slot_config) {
    esp_err_t ret;

    ESP_LOGI(TAG, "mounting FAT filesystem");
    ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, host, slot_config, mount_config,
                                  &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "failed to mount filesystem");
        } else {
            ESP_LOGE(TAG, "failed to initialize the card: %s",
                     esp_err_to_name(ret));
        }

        return ret;
    }

    ESP_LOGI(TAG, "filesystem mounted");
    sd_card_mounted = true;

    sdmmc_card_print_info(stdout, card);

    return ret;
}

esp_err_t mount_sdmmc_card_1w(int clk_pin, int cmd_pin, int d0_pin) {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = init_mount_config();
    ESP_LOGI(TAG, "initializing SD card as an SDMMC peripheral");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    // optional, default is 20KHz, high speed is 40KHz
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;
    slot_config.clk = clk_pin;
    slot_config.cmd = cmd_pin;
    slot_config.d0 = d0_pin;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    return mount_sdmmc_card(&mount_config, &host, &slot_config);
}

esp_err_t mount_sdmmc_card_4w(int clk_pin, int cmd_pin, int d0_pin, int d1_pin,
                              int d2_pin, int d3_pin) {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = init_mount_config();
    ESP_LOGI(TAG, "initializing SD card as an SDMMC peripheral");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    // optional, default is 20KHz, high speed is 40KHz
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    slot_config.clk = clk_pin;
    slot_config.cmd = cmd_pin;
    slot_config.d0 = d0_pin;
    slot_config.d1 = d1_pin;
    slot_config.d2 = d2_pin;
    slot_config.d3 = d3_pin;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    return mount_sdmmc_card(&mount_config, &host, &slot_config);
}

esp_err_t mount_sdspi_card(int cs_pin) {
    esp_err_t ret;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = init_mount_config();
    ESP_LOGI(TAG, "initializing SD card as an SPI peripheral");

    // SPI bus init is handled by the spi manager
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = cs_pin;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "mounting FAT filesystem");
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config,
                                  &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "failed to mount filesystem");
        } else {
            ESP_LOGE(TAG, "failed to initialize the card: %s",
                     esp_err_to_name(ret));
        }

        return ret;
    }

    ESP_LOGI(TAG, "filesystem mounted");
    sd_card_mounted = true;

    sdmmc_card_print_info(stdout, card);

    return ret;
}

esp_err_t write_to_file(const char *file_path, const char *data) {
    if (!sd_card_mounted) {
        ESP_LOGE(TAG, "SD card not mounted, aborting operation");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "opening file %s for writing", file_path);

    FILE *f = fopen(file_path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "failed to open file %s for writing", file_path);
        return ESP_FAIL;
    }

    fprintf(f, "%s", data);
    fclose(f);

    ESP_LOGI(TAG, "data successfully written to file");

    return ESP_OK;
}

esp_err_t read_file(const char *file_path) {
    if (!sd_card_mounted) {
        ESP_LOGE(TAG, "SD card not mounted, aborting operation");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "reading from file %s", file_path);
    FILE *f = fopen(file_path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "failed to open file %s for reading", file_path);
        return ESP_FAIL;
    }
    char line[MAX_CHAR_SIZE];
    fgets(line, sizeof(line), f);
    fclose(f);

    // strip newline
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "file contents: '%s'", line);

    return ESP_OK;
}

esp_err_t read_dir_contents(const char *dir_path) {
    if (!sd_card_mounted) {
        ESP_LOGE(TAG, "SD card not mounted, aborting operation");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "listing files in directory %s", dir_path);

    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        ESP_LOGE(TAG, "failed to open directory %s", dir_path);
        return ESP_FAIL;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        printf("%s\n", entry->d_name);
    }

    closedir(dir);
    return ESP_OK;
}

static void scale_image_simple(uint8_t *src_buf, int src_width, int src_height,
                               uint8_t *dst_buf, int dst_width,
                               int dst_height) {
    for (int y = 0; y < dst_height; y++) {
        for (int x = 0; x < dst_width; x++) {
            int src_x = (x * src_width) / dst_width;
            int src_y = (y * src_height) / dst_height;

            int src_idx = (src_y * src_width + src_x) * 2;
            int dst_idx = (y * dst_width + x) * 2;

            dst_buf[dst_idx] = src_buf[src_idx];
            dst_buf[dst_idx + 1] = src_buf[src_idx + 1];
        }
    }
}

esp_err_t load_and_display_image(const char *filepath) {
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        ESP_LOGE(TAG, "failed to open %s", filepath);
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    ESP_LOGI(TAG, "JPEG file size: %zu bytes", file_size);

    uint8_t *jpeg_buf = heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM);
    if (!jpeg_buf) {
        fclose(f);
        ESP_LOGE(TAG, "failed to allocate JPEG buffer");
        return ESP_ERR_NO_MEM;
    }

    size_t bytes_read = fread(jpeg_buf, 1, file_size, f);
    fclose(f);

    if (bytes_read != file_size) {
        ESP_LOGE(TAG, "failed to read complete file");
        heap_caps_free(jpeg_buf);
        return ESP_FAIL;
    }

    size_t img_buf_size = 640 * 480 * 2;
    uint8_t *rgb_buf = heap_caps_malloc(img_buf_size, MALLOC_CAP_SPIRAM);
    if (!rgb_buf) {
        ESP_LOGE(TAG, "failed to allocate RGB buffer");
        heap_caps_free(jpeg_buf);
        return ESP_ERR_NO_MEM;
    }

    esp_jpeg_image_cfg_t decode_cfg = {
        .indata = jpeg_buf,
        .indata_size = file_size,
        .outbuf = rgb_buf,
        .outbuf_size = img_buf_size,
        .out_format = JPEG_IMAGE_FORMAT_RGB565,
        .out_scale =
            JPEG_IMAGE_SCALE_1_8}; // TODO try other scales with the OV5640

    esp_jpeg_image_output_t outimg;
    esp_err_t ret = esp_jpeg_decode(&decode_cfg, &outimg);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "decoded: %dx%d, %d bytes", outimg.width, outimg.height,
                 outimg.output_len);

        const int DISPLAY_WIDTH = 320;
        const int DISPLAY_HEIGHT = 240;
        size_t display_buf_size = DISPLAY_WIDTH * DISPLAY_HEIGHT * 2;

        uint8_t *display_buf =
            heap_caps_malloc(display_buf_size, MALLOC_CAP_SPIRAM);
        if (display_buf) {
            scale_image_simple(rgb_buf, outimg.width, outimg.height,
                               display_buf, DISPLAY_WIDTH, DISPLAY_HEIGHT);

            camera_widget_set_feed(display_buf, display_buf_size);

            heap_caps_free(display_buf);
        }
    }

    heap_caps_free(rgb_buf);
    heap_caps_free(jpeg_buf);
    return ret;
}

void unmount_sd_card(void) {
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
    sd_card_mounted = false;
    ESP_LOGI(TAG, "SD card unmounted");
}

esp_err_t format_sd_card(void) {
    esp_err_t ret;

    ret = esp_vfs_fat_sdcard_format(MOUNT_POINT, card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to format FATFS: %s", esp_err_to_name(ret));
    }

    return ret;
}

// we can use POSIX and C stdlib functions to work with files
void sd_card_task(void *params) {
    esp_err_t ret;

    const char *file_foo = MOUNT_POINT "/foo.txt";
    char data[MAX_CHAR_SIZE];
    snprintf(data, MAX_CHAR_SIZE, "%s %s!\n", "FOOOOOOOOO", card->cid.name);
    ret = write_to_file(file_foo, data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write failed");
        vTaskDelete(NULL);
        return;
    }

    const char *file_bar = MOUNT_POINT "/bar.txt";

    // check if bar file exists before renaming
    struct stat st;
    if (stat(file_bar, &st) == 0) {
        // delete it if it exists, just for the sake of testing
        unlink(file_bar);
    }

    ESP_LOGI(TAG, "renaming file %s to %s", file_foo, file_bar);
    if (rename(file_foo, file_bar) != 0) {
        ESP_LOGE(TAG, "rename failed");
        vTaskDelete(NULL);
        return;
    }

    ret = read_file(file_bar);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read failed");
        vTaskDelete(NULL);
        return;
    }

    const char *dir_name = MOUNT_POINT;
    ret = read_dir_contents(dir_name);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "dir content read failed");
        vTaskDelete(NULL);
        return;
    }

    vTaskDelete(NULL);
}

void run_sd_card_task(void) {
    if (sd_card_mounted) {
        xTaskCreate(sd_card_task, "sd_card_task", 4096, NULL, 1, NULL);
    } else {
        ESP_LOGE(TAG, "SD card hasn't been mounted!!");
    }
}
