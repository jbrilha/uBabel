#include "loudness_sensor.h"

#include "adc_hal.h"

const static char *TAG = "LOUDNESS_SENSOR";

static int sensor_pin;

bool loudness_sensor_init(int pin) {
    if (adc_init_pin(pin)) {
        sensor_pin = pin;
        return true;
    }

    return false;
}

int loudness_sensor_get_reading(void) { return adc_get_pin_value(sensor_pin); }
