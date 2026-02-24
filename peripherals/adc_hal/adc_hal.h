#ifndef ADC_HAL_H
#define ADC_HAL_H

#include <stdbool.h>

bool adc_init_pin(int pin);
int adc_get_pin_value(int pin);
void adc_deinit_pin(int pin);

#endif // !ADC_HAL_H
