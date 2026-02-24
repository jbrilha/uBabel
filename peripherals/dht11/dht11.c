#include "dht11.h"
#include <stdio.h>

#include "gpio_hal.h"
#include "time_hal.h"
#include "platform.h"

#define DHT_TASK_NAME "dht_task"
#define DHT_TASK_STACK_SIZE 4096
#define DHT_TASK_PRIORITY 1

#ifdef BUILD_ESP32
#define DHT_PIN 22 // just as an example, works for the M5

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
#define PORT_ENTER_CRITICAL() portENTER_CRITICAL(&mux)
#define PORT_EXIT_CRITICAL() portEXIT_CRITICAL(&mux)

#else
#define DHT_PIN 16

// these bug out the pico, even with the freertos portmacros
#define PORT_ENTER_CRITICAL()
#define PORT_EXIT_CRITICAL()
#endif

#define DHT_TIMER_INTERVAL 2
#define DHT_DATA_BITS 40
#define DHT_DATA_BYTES (DHT_DATA_BITS / 8)

#define CHECK_LOGE(x, msg, ...)                                                \
    do {                                                                       \
        bool __;                                                               \
        if ((__ = x) != true) {                                                \
            PORT_EXIT_CRITICAL();                                              \
            LOG_ERROR(TAG, msg, ##__VA_ARGS__);                                \
            return __;                                                         \
        }                                                                      \
    } while (0)

static const char *TAG = "DHT11";

static int dht_pin;

static bool dht_await_pin_state(uint32_t timeout, int expected_pin_state,
                                uint32_t *duration) {
    gpio_set_pin_dir(dht_pin, GPIO_INPUT);
    for (uint32_t i = 0; i < timeout; i += DHT_TIMER_INTERVAL) {
        // need to wait at least a single interval to prevent reading a jitter
        hal_sleep_us(DHT_TIMER_INTERVAL);
        if (gpio_get_pin_level(dht_pin) == expected_pin_state) {
            if (duration)
                *duration = i;
            return true;
        }
    }

    return false;
}

static inline bool dht_fetch_data(uint8_t data[DHT_DATA_BYTES]) {
    uint32_t low_duration;
    uint32_t high_duration;

    // Phase 'A' pulling signal low to initiate read sequence
    gpio_set_pin_dir(dht_pin, GPIO_OUTPUT);
    gpio_set_pin_level(dht_pin, 0);
    hal_sleep_us(20000);
    gpio_set_pin_level(dht_pin, 1);

    // Step through Phase 'B', 40us
    CHECK_LOGE(dht_await_pin_state(40, 0, NULL),
               "Initialization error, problem in phase 'B'");
    // Step through Phase 'C', 88us
    CHECK_LOGE(dht_await_pin_state(88, 1, NULL),
               "Initialization error, problem in phase 'C'");
    // Step through Phase 'D', 88us
    CHECK_LOGE(dht_await_pin_state(88, 0, NULL),
               "Initialization error, problem in phase 'D'");

    // Read in each of the 40 bits of data...
    for (int i = 0; i < DHT_DATA_BITS; i++) {
        CHECK_LOGE(dht_await_pin_state(65, 1, &low_duration),
                   "LOW bit timeout");
        CHECK_LOGE(dht_await_pin_state(75, 0, &high_duration),
                   "HIGH bit timeout");

        uint8_t b = i / 8;
        uint8_t m = i % 8;
        if (!m)
            data[b] = 0;

        data[b] |= (high_duration > low_duration) << (7 - m);
    }

    return true;
}

static inline int16_t dht_convert_data(uint8_t msb, uint8_t lsb) {
    int16_t data;

    data = msb * 10;

    return data;
}

bool dht_read_data(int16_t *humidity, int16_t *temperature) {

    uint8_t data[DHT_DATA_BYTES] = {0};

    gpio_set_pin_dir(dht_pin, GPIO_OUTPUT);
    gpio_set_pin_level(dht_pin, 1);

    PORT_ENTER_CRITICAL();
    bool result = dht_fetch_data(data);
    if (result) {
        PORT_EXIT_CRITICAL();
    }

    gpio_set_pin_dir(dht_pin, GPIO_OUTPUT);
    gpio_set_pin_level(dht_pin, 1);

    if (!result)
        return result;

    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
        LOG_ERROR(TAG, "Checksum failed, invalid data received from sensor");
        return false;
    }

    if (humidity)
        *humidity = dht_convert_data(data[0], data[1]);
    if (temperature)
        *temperature = dht_convert_data(data[2], data[3]);

    return true;
}

bool dht_read_float_data(float *humidity, float *temperature) {
    int16_t i_humidity, i_temp;

    bool res = dht_read_data(humidity ? &i_humidity : NULL,
                             temperature ? &i_temp : NULL);
    if (!res)
        return res;

    if (humidity)
        *humidity = i_humidity / 10.0;
    if (temperature)
        *temperature = i_temp / 10.0;

    return true;
}

void dht_init(int pin) {
    dht_pin = pin;
    gpio_init_pin(pin);
}

void dht_task(void *params) {
    int pin = (int)params;
    dht_init(pin);

    float humidity = 0, temp_celsius = 0;

    while (1) {
        if (dht_read_float_data(&humidity, &temp_celsius) == true)
            LOG_INFO(TAG, "Humidity: %.1f%% Temp: %.1fC\n", humidity,
                     temp_celsius);
        else
            LOG_ERROR(TAG, "Could not read data from sensor\n");

        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    vTaskDelete(NULL);
}

void run_dht(void) {
    xTaskCreate(dht_task, DHT_TASK_NAME, DHT_TASK_STACK_SIZE, (void *)DHT_PIN,
                DHT_TASK_PRIORITY, NULL);
}
