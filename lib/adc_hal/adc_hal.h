#ifndef GPIO_HAL_H
#define GPIO_HAL_H

#ifdef BUILD_ESP32
#include "esp_adc/adc_oneshot.h"

#else
#include "hardware/adc.h"

#endif

bool adc_init_pin(int pin);
int adc_get_pin_value(int pin);

#endif // !GPIO_HAL_H
