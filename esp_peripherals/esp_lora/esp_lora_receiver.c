#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <driver/spi_master.h>

#include "esp_lora_receiver.h"

#include "lora_types.h"
#include "lora_events.h"
#include "event_dispatcher.h"

static const char *TAG = "SX127X_RECEIVER";

static int total_packets_received = 0;

void rx_callback(sx127x *device, uint8_t *data, uint16_t data_length) {
    if (data_length < sizeof(lora_pkt_t)) {
        ESP_LOGW(TAG, "Packet too small: %d bytes", data_length);
        return;
    }

    lora_pkt_t *pkt = (lora_pkt_t *)data;

    size_t expected_size = sizeof(lora_pkt_t) + pkt->payload_len;
    if (data_length != expected_size) {
        ESP_LOGW(TAG, "Size mismatch: got %d, expected %zu", data_length,
                 expected_size);
        return;
    }

    int16_t rssi;
    ESP_ERROR_CHECK(sx127x_rx_get_packet_rssi(device, &rssi));
    float snr;
    ESP_ERROR_CHECK(sx127x_lora_rx_get_packet_snr(device, &snr));
    int32_t frequency_error;
    ESP_ERROR_CHECK(sx127x_rx_get_frequency_error(device, &frequency_error));

    char payload_str[pkt->payload_len + 1];
    memcpy(payload_str, pkt->payload, pkt->payload_len);
    payload_str[pkt->payload_len] = '\0';

    ESP_LOGI(TAG,
             "RX: from=0x%02X to=0x%02X msg_id=%d payload='%s' rssi=%d "
             "snr=%.1f freq_err=%" PRId32,
             pkt->sender_id, pkt->recipient_id, pkt->message_id, payload_str,
             rssi, snr, frequency_error);

    lora_info_t *info = (lora_info_t *)malloc(sizeof(lora_info_t) + data_length);
    info->rssi = rssi;
    info->snr = snr;
    info->freq_err = frequency_error;
    info->pkt_size = data_length;

    uint8_t *copied_data = (uint8_t *)(info + 1);
    memcpy(copied_data, data, data_length);
    info->pkt = (lora_pkt_t *)copied_data;

    event_t *event = create_event(EVENT_TYPE_NOTIFICATION, UI_EVENT_REC_LORA,
                                  info, sizeof(lora_info_t) + data_length);
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
