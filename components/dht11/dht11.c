#include "dht11.h"
#include "dht.h"
#include <malloc.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "gpio_hal.h"

static const char *TAG = "DHT";

#define DHT_TASK_NAME "dht_task"
#define DHT_TASK_STACK_SIZE 4096
#define DHT_TASK_PRIORITY 1

#define DHT_PIN 22

void dht_init(int pin) { gpio_init_pin(pin); }

void dht_task(void *params) {
    dht_init(DHT_PIN);

    dht11_reading_t r;

    while (1) {
        if (dht_read_float_data(0, 22, &r.humidity, &r.temp_celsius) == ESP_OK)
            ESP_LOGI(TAG, "Humidity: %.1f%% Temp: %.1fC\n", r.humidity,
                     r.temp_celsius);
        else
            ESP_LOGE(TAG, "Could not read data from sensor\n");

        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    vTaskDelete(NULL);
}

void run_dht(void) {
    xTaskCreate(dht_task, DHT_TASK_NAME, DHT_TASK_STACK_SIZE, NULL,
                DHT_TASK_PRIORITY, NULL);
}
