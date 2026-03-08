#include "gpio_hal.h"

void gpio_init_pin(int pin) {
    gpio_init(pin);
    gpio_disable_pulls(pin); // this matches the ESP config but I'm not 100%
                             // sure it's necessary
}

void gpio_set_pin_dir(int pin, int dir) { gpio_set_dir(pin, dir); }

// true = high
// false = low
void gpio_set_pin_level(int pin, bool high) { gpio_put(pin, high); }

int gpio_get_pin_level(int pin) { return gpio_get(pin); }

void gpio_toggle_pin(int pin) { gpio_set(pin, !gpio_get(pin)); }
