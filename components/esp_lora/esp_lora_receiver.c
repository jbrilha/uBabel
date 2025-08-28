#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <driver/spi_master.h>

#include "esp_lora_receiver.h"

#include "event.h"
#include "event_dispatcher.h"

static const char *TAG = "SX127X_RECEIVER";

static int total_packets_received = 0;

void rx_callback(sx127x *device, uint8_t *data, uint16_t data_length) {
    uint8_t payload[514];
    const char SYMBOLS[] = "0123456789ABCDEF";
    for (size_t i = 0; i < data_length; i++) {
        uint8_t cur = data[i];
        payload[2 * i] = SYMBOLS[cur >> 4];
        payload[2 * i + 1] = SYMBOLS[cur & 0x0F];
    }
    payload[data_length * 2] = '\0';

    int16_t rssi;
    ESP_ERROR_CHECK(sx127x_rx_get_packet_rssi(device, &rssi));
    float snr;
    ESP_ERROR_CHECK(sx127x_lora_rx_get_packet_snr(device, &snr));
    int32_t frequency_error;
    ESP_ERROR_CHECK(sx127x_rx_get_frequency_error(device, &frequency_error));
    ESP_LOGI(TAG, "received: %d %s rssi: %d snr: %f freq_error: %" PRId32,
             data_length, payload, rssi, snr, frequency_error);

    // TODO THIS MIGHT MEMORY LEAK!!
    lora_payload_t *lp = malloc(sizeof(lora_payload_t) + data_length);
    lp->length = data_length;
    lp->rssi = rssi;
    lp->snr = snr;
    lp->freq_err = frequency_error;
    memcpy(lp->payload, payload, data_length);

    event_t *event = create_event(EVENT_TYPE_NOTIFICATION, UI_EVENT_REC_LORA,
                                  lp, sizeof(lora_payload_t) + data_length);
    if (event) {
        event_dispatcher_post(event);
    }
    total_packets_received++;
}

void cad_callback(sx127x *device, int cad_detected) {
    if (cad_detected == 0) {
        ESP_LOGI(TAG, "cad not detected");
        ESP_ERROR_CHECK(
            sx127x_set_opmod(SX127x_MODE_CAD, SX127x_MODULATION_LORA, device));
        return;
    }
    // put into RX mode first to handle interrupt as soon as possible
    ESP_ERROR_CHECK(
        sx127x_set_opmod(SX127x_MODE_RX_CONT, SX127x_MODULATION_LORA, device));
    ESP_LOGI(TAG, "cad detected\n");
}

void configure_receiver(sx127x *device) {
    sx127x_rx_set_callback(rx_callback, device);
    sx127x_lora_cad_set_callback(cad_callback, device);

    ESP_ERROR_CHECK(
        sx127x_set_opmod(SX127x_MODE_RX_CONT, SX127x_MODULATION_LORA, device));
}
