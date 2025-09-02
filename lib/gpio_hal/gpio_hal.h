#ifndef GPIO_HAL_H
#define GPIO_HAL_H

#include <stdbool.h>

#ifdef BUILD_ESP32
#include <driver/gpio.h>
#include <rom/ets_sys.h>
#else
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#endif

void gpio_init_pin(int pin);

void gpio_set_pin_dir(int pin, int dir);

void gpio_set_pin_level(int pin, bool high);

int gpio_get_pin_level(int pin);

void hal_sleep_us(int us);

#endif // !GPIO_HAL_H
