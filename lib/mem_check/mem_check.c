#include "mem_check.h"
#include <malloc.h>
#include <stdio.h>

#define MEM_CHECK_TASK_STACK_SIZE 1024
#define MEM_CHECK_TASK_PRIORITY 1

static const char *TAG = "MEM_CHECK";

#ifdef BUILD_ESP32
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"

void mem_check(void) {
    size_t total_sram = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    LOG_INFO(TAG, "SRAM:\t\t%d / %d KB", free_sram / 1024, total_sram / 1024);

    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    LOG_INFO(TAG, "PSRAM:\t\t%d / %d KB", free_psram / 1024,
             total_psram / 1024);

    size_t free_heap = esp_get_free_heap_size();
    size_t total_heap = total_psram + total_sram;
    LOG_INFO(TAG, "Heap:\t\t%d / %d KB", free_heap / 1024, total_heap / 1024);

    uint32_t flash_size = 0;
    esp_err_t ret = esp_flash_get_size(NULL, &flash_size);
    if (ret == ESP_OK) {
        LOG_INFO(TAG, "Flash size: %d KB", flash_size / 1024);
    } else {
        LOG_INFO(TAG, "Flash size: Could not detect");
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    LOG_INFO(TAG, "App partition size: %d KB", running->size / 1024);
}
#elif defined(BUILD_PICO)

void mem_check(void) {
#ifdef PICO_RP2350
    bool is_2350 = true;
#else
    bool is_2350 = false;
#endif

    extern char __StackLimit;
    extern char __bss_end__;

    size_t total_heap = is_2350 ? 512 : 256;
    size_t free_heap = &__StackLimit - &__bss_end__;
    LOG_INFO(TAG, "Heap:\t\t%d / %d KB", free_heap / 1024, total_heap);

    LOG_INFO(TAG, "Flash size:\t%d KB\n", PICO_FLASH_SIZE_BYTES / 1024);

    extern char __flash_binary_start;
    extern char __flash_binary_end;

    size_t app_size = (size_t)(&__flash_binary_end - &__flash_binary_start);
    LOG_INFO(TAG, "App size:\t%zu KB\n", app_size / 1024);
}
#endif

void mem_check_task(void *params) {
    mem_check();

    vTaskDelete(NULL);
}
