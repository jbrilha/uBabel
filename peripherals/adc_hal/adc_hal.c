#include "adc_hal.h"
#include "platform.h"

#ifdef BUILD_ESP32
#include "esp_adc/adc_oneshot.h"

static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_oneshot_unit_handle_t adc2_handle = NULL;

#if CONFIG_IDF_TARGET_ESP32
#define MIN_GPIO 2
#define MAX_GPIO 39

// clang-format off
static const int gpio_to_adc_channel[] = {
    // ADC1
    [36] = ADC_CHANNEL_0,
    [39] = ADC_CHANNEL_3,
    [32] = ADC_CHANNEL_4,
    [33] = ADC_CHANNEL_5,
    [34] = ADC_CHANNEL_6,
    [35] = ADC_CHANNEL_7,
    // ADC2
    [4] = ADC_CHANNEL_0,
    [2] = ADC_CHANNEL_2,
    [15] = ADC_CHANNEL_3,
    [13] = ADC_CHANNEL_4,
    [12] = ADC_CHANNEL_5,
    [14] = ADC_CHANNEL_6,
    [27] = ADC_CHANNEL_7,
    [25] = ADC_CHANNEL_8,
    [26] = ADC_CHANNEL_9,
};
// clang-format on

#elif CONFIG_IDF_TARGET_ESP32S3
#define MIN_GPIO 1
#define MAX_GPIO 20

// clang-format off
static const int gpio_to_adc_channel[] = {
    [1] = ADC_CHANNEL_0,
    [2] = ADC_CHANNEL_1,
    [3] = ADC_CHANNEL_2,
    [4] = ADC_CHANNEL_3,
    [5] = ADC_CHANNEL_4,
    [6] = ADC_CHANNEL_5,
    [7] = ADC_CHANNEL_6,
    [8] = ADC_CHANNEL_7,
    [9] = ADC_CHANNEL_8,
    [10] = ADC_CHANNEL_9,
};
// clang-format on

#else
// TODO FOR OTHER BOARDS
#define MIN_GPIO 1
#define MAX_GPIO 20

static const int gpio_to_adc_channel[] = {
};
#endif

#elif BUILD_PICO
#include "hardware/adc.h"

#define MIN_GPIO 26
#define MAX_GPIO 28

static const int gpio_to_adc_channel[] = {
    [26] = 0, // GPIO26 -> ADC0
    [27] = 1, // GPIO27 -> ADC1
    [28] = 2, // GPIO28 -> ADC2
};
#endif

const static char *TAG = "ADC_HAL";

static bool adc_initialized = false;

static int get_adc_channel_for_gpio(int gpio) {
    if (gpio < MIN_GPIO || gpio > MAX_GPIO) {
        LOG_ERROR(TAG, "provided GPIO pin %d does not have an ADC channel",
                  gpio);
        return -1;
    }

#if CONFIG_IDF_TARGET_ESP32S3
    if (gpio > 10) {
        gpio -= 10; // 1 and 11 have the same channel, 2 and 12 etc
    }
#endif

    return gpio_to_adc_channel[gpio];
}

bool adc_init_pin(int pin) {
    int chan = get_adc_channel_for_gpio(pin);
    if (chan < 0) {
        return false;
    }

#if BUILD_ESP32
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    if (pin <= 10) {
        adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = ADC_UNIT_1,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, chan, &config));
    } else {
        adc_oneshot_unit_init_cfg_t init_config2 = {
            .unit_id = ADC_UNIT_2,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config2, &adc2_handle));

        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, chan, &config));
    }

    return true;
#else
    if (!adc_initialized) {
        adc_init();
        adc_initialized = true;
    }

    // this makes sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(pin);
    return true;
#endif
}

int adc_get_pin_value(int pin) {
    int chan = get_adc_channel_for_gpio(pin);
    if (chan < 0) {
        return -1;
    }

#if BUILD_ESP32
    int val;
    if (pin <= 10) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, chan, &val));
    } else {
        ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, chan, &val));
    }

    return val;
#else
    adc_select_input(chan);

    return adc_read();
#endif
}

#if BUILD_ESP32
void adc_deinit_pin(int pin) {
    if (pin <= 10) {
        ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    } else {
        ESP_ERROR_CHECK(adc_oneshot_del_unit(adc2_handle));
    }
}
#endif
