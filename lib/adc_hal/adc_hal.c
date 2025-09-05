#include "adc_hal.h"

#include "platform.h"

const static char *TAG = "ADC_HAL";

static bool adc_initialized = false;

#if BUILD_ESP32
#define ADC_PIN 7

#if CONFIG_IDF_TARGET_ESP32
#define ADC_PIN_7 ADC_CHANNEL_4
#else
#define ADC_PIN_7 ADC_CHANNEL_6
#endif

static adc_oneshot_unit_handle_t adc1_handle = NULL;
#else
static const int gpio_to_adc_channel[] = {
    [26] = 0, // GPIO26 -> ADC0
    [27] = 1, // GPIO27 -> ADC1
    [28] = 2, // GPIO28 -> ADC2
};
#endif

static  int adc_pin = -1;

bool adc_init_pin(int pin) {
#if BUILD_ESP32
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_ERROR_CHECK(
        adc_oneshot_config_channel(adc1_handle, ADC_PIN_7, &config));

    return true;
#else
    if (pin < 26 || pin > 28) {
        LOG_ERROR(TAG, "provided GPIO pin %d does not have an ADC channel",
                  pin);
        return false;
    }

    if (!adc_initialized) {
        adc_init();
        adc_initialized = true;
    }

    // this makes sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(pin);
    return true;
#endif
}

static int get_adc_channel_for_gpio(int gpio) {
#if BUILD_ESP32
#else
    if (gpio < 26 || gpio > 28) {
        LOG_ERROR(TAG, "provided GPIO pin %d does not have an ADC channel",
                  gpio);
        return -1;
    }

    return gpio_to_adc_channel[gpio];
#endif
}

int adc_get_pin_value(int pin) {
#if BUILD_ESP32
    int val;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_PIN_7, &val));

    return val;
#else
    int chan = get_adc_channel_for_gpio(pin);
    if (chan < 0) {
        return -1;
    }

    adc_select_input(chan);

    return adc_read();
#endif
}

