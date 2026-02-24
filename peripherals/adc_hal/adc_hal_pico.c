#include "adc_hal.h"
#include "platform.h"

#include "hardware/adc.h"

#define MIN_GPIO 26
#define MAX_GPIO 28

static const int gpio_to_adc_channel[] = {
    [26] = 0, // GPIO26 -> ADC0
    [27] = 1, // GPIO27 -> ADC1
    [28] = 2, // GPIO28 -> ADC2
};

const static char *TAG = "ADC_HAL";

static bool adc_initialized = false;

static int get_adc_channel_for_gpio(int gpio) {
    if (gpio < MIN_GPIO || gpio > MAX_GPIO) {
        LOG_ERROR(TAG, "provided GPIO pin %d does not have an ADC channel",
                  gpio);
        return -1;
    }

    return gpio_to_adc_channel[gpio];
}

bool adc_init_pin(int pin) {
    int chan = get_adc_channel_for_gpio(pin);
    if (chan < 0) {
        return false;
    }

    if (!adc_initialized) {
        adc_init();
        adc_initialized = true;
    }

    // this makes sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(pin);
    return true;
}

int adc_get_pin_value(int pin) {
    int chan = get_adc_channel_for_gpio(pin);
    if (chan < 0) {
        return -1;
    }

    adc_select_input(chan);

    return adc_read();
}

void adc_deinit_pin(int pin) {
    // does nothing on the Pico
}
