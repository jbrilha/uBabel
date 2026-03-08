#include "driver/gpio.h"
#include "gpio_hal.h"

void gpio_init_pin(int pin) {
    gpio_config_t io_conf = {.pin_bit_mask = (1ULL << pin),
                             .pull_up_en = GPIO_PULLUP_DISABLE,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);
}

void gpio_set_pin_dir(int pin, int dir) { gpio_set_direction(pin, dir); }

void gpio_set_pin_level(int pin, bool high) { gpio_set_level(pin, high); }

int gpio_get_pin_level(int pin) { return gpio_get_level(pin); }

void gpio_toggle_pin(int pin) { gpio_set_level(pin, !gpio_get_level(pin)); }
