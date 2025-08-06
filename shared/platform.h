#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef BUILD_PICO
static const char *TAG = "PICO";

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"


#elif defined(BUILD_ESP32)
static const char *TAG = "ESP32";

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"

#include "esp_log.h"

#define LOG_INFO(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define LOG_ERROR(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#define LOG_WARN(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#endif

#ifndef BUILD_ESP32
#define LOG_INFO(tag, fmt, ...) printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
#define LOG_ERROR(tag, fmt, ...)                                               \
    printf("[ERROR %s] " fmt "\n", tag, ##__VA_ARGS__)
#define LOG_WARN(tag, fmt, ...)                                               \
    printf("[WARNING %s] " fmt "\n", tag, ##__VA_ARGS__)
#endif

#endif // PLATFORM_H
