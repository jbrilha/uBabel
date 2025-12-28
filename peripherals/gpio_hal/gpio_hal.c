#include "gpio_hal.h"

void gpio_init_pin(int pin) {
#if BUILD_ESP32
    gpio_config_t io_conf = {.pin_bit_mask = (1ULL << pin),
                             .pull_up_en = GPIO_PULLUP_DISABLE,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);
#else
    gpio_init(pin);
    gpio_disable_pulls(pin); // this matches the ESP config but I'm not 100%
                             // sure it's necessary
#endif
}

void gpio_set_pin_dir(int pin, int dir) {
#if BUILD_ESP32
    gpio_set_direction(pin, dir);
#else
    gpio_set_dir(pin, dir);
#endif
}

// true = high
// false = low
void gpio_set_pin_level(int pin, bool high) {
#if BUILD_ESP32
    gpio_set_level(pin, high);
#else
    gpio_put(pin, high);
#endif
}

int gpio_get_pin_level(int pin) {
#if BUILD_ESP32
    return gpio_get_level(pin);
#else
    return gpio_get(pin);
#endif
}

// TODO move these into non-gpio HAL
void hal_sleep_us(int us) {
#if BUILD_ESP32
    ets_delay_us(us);
#else
    sleep_us(us);
#endif
}

uint32_t hal_millis(void) {
#if BUILD_ESP32
    return esp_timer_get_time() / 1000;
#else
    return to_ms_since_boot(get_absolute_time());
#endif
}
