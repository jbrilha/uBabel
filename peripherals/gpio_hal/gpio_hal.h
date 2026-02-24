#ifndef GPIO_HAL_H
#define GPIO_HAL_H

#include <stdbool.h>

#ifdef BUILD_ESP32
#include <driver/gpio.h>

typedef enum {
    GPIO_INPUT = GPIO_MODE_INPUT,
    GPIO_OUTPUT = GPIO_MODE_OUTPUT,
} gpio_hal_mode_t;
#else
#include "pico/stdlib.h"
#include "hardware/gpio.h"

typedef enum {
    GPIO_INPUT = GPIO_IN,
    GPIO_OUTPUT = GPIO_OUT,
} gpio_hal_mode_t;

#endif

void gpio_init_pin(int pin);

void gpio_set_pin_dir(int pin, int dir);

void gpio_set_pin_level(int pin, bool high);

int gpio_get_pin_level(int pin);

#endif // !GPIO_HAL_H
