#include "esp_lora_sender.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"

#define TX_INTERVAL_MS 2000

static const char *TAG = "SX127X_SENDER";

static bool ready = true;

static const int supported_power_levels[] = {2,  3,  4,  5,  6,  7,  8,  9, 10,
                                             11, 12, 13, 14, 15, 16, 17, 20};
static const int supported_power_levels_count =
    sizeof(supported_power_levels) / sizeof(int);
static int current_power_level = 0;

static int messages_sent = 0;

void tx_callback(sx127x *device) {
    if (messages_sent > 0) {
        ESP_LOGI(TAG, "transmitted");
        vTaskDelay(pdMS_TO_TICKS(TX_INTERVAL_MS));
    }

    if (messages_sent == 0) {
        uint8_t data[] = {0xCA, 0xFE};

        ESP_ERROR_CHECK(
            sx127x_lora_tx_set_for_transmission(data, sizeof(data), device));
    } else if (current_power_level < supported_power_levels_count) {
        uint8_t data[] = {0xCA, 0xFE};

        ESP_ERROR_CHECK(
            sx127x_lora_tx_set_for_transmission(data, sizeof(data), device));
        ESP_ERROR_CHECK(sx127x_tx_set_pa_config(
            SX127x_PA_PIN_BOOST, supported_power_levels[current_power_level],
            device));

        current_power_level++;
    } else {
        current_power_level = 0; // reset power level cycle
        uint8_t data[] = {0xCA, 0xFE};

        ESP_ERROR_CHECK(
            sx127x_lora_tx_set_for_transmission(data, sizeof(data), device));
        ESP_ERROR_CHECK(sx127x_tx_set_pa_config(
            SX127x_PA_PIN_BOOST, supported_power_levels[current_power_level],
            device));

        current_power_level++;
    }
    ESP_ERROR_CHECK(
        sx127x_set_opmod(SX127x_MODE_TX, SX127x_MODULATION_LORA, device));
    ESP_LOGI(TAG, "transmitting");
    messages_sent++;
}

void configure_sender(sx127x *device) {
    sx127x_tx_set_callback(tx_callback, device);

    ESP_ERROR_CHECK(sx127x_tx_set_pa_config(
        SX127x_PA_PIN_BOOST, supported_power_levels[current_power_level],
        device));
    sx127x_tx_header_t header = {.enable_crc = true,
                                 .coding_rate = SX127x_CR_4_5};

    ESP_ERROR_CHECK(sx127x_lora_tx_set_explicit_header(&header, device));
}
