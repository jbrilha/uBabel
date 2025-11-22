#include "esp_lora_sender.h"

#include "event_dispatcher.h"
#include "lora_events.h"
#include "lora_types.h"

#define TX_INTERVAL_MS 2000

static const char *TAG = "SX127X_SENDER";

static bool ready = true;

static const int supported_power_levels[] = {2,  3,  4,  5,  6,  7,  8,  9, 10,
                                             11, 12, 13, 14, 15, 16, 17, 20};
static const int supported_power_levels_count =
    sizeof(supported_power_levels) / sizeof(int);
static int current_power_level = 0;

static int messages_sent = 0;

static uint8_t my_id = 0xBB;
static uint8_t recipient_id = 0xAB;

static lora_pkt_t *new_packet(int id, int power_level) {
    char payload[32];
    snprintf(payload, sizeof(payload), "Hello with power level: %d",
             power_level);

    size_t payload_len = strlen(payload);
    size_t total_size = sizeof(lora_pkt_t) + payload_len;

    lora_pkt_t *pkt = (lora_pkt_t *)malloc(total_size);
    pkt->recipient_id = recipient_id;
    pkt->sender_id = my_id;
    pkt->message_id = id;
    pkt->payload_len = payload_len;
    memcpy(pkt->payload, payload, payload_len);

    return pkt;
}

void tx_callback(void *ctx) {
    if (messages_sent > 0) {
        ESP_LOGI(TAG, "transmitted packet with id: %d", messages_sent);
        vTaskDelay(pdMS_TO_TICKS(TX_INTERVAL_MS));
    }

    messages_sent++;
}

void transmit_packet_from_device(sx127x *device) {
    lora_pkt_t *pkt =
        new_packet(messages_sent, supported_power_levels[current_power_level]);
    size_t pkt_size = sizeof(lora_pkt_t) + pkt->payload_len;

    ESP_ERROR_CHECK(
        sx127x_lora_tx_set_for_transmission((uint8_t *)pkt, pkt_size, device));
    ESP_ERROR_CHECK(sx127x_tx_set_pa_config(
        SX127X_PA_PIN_BOOST, supported_power_levels[current_power_level],
        device));

    current_power_level =
        (current_power_level + 1) % supported_power_levels_count;

    ESP_ERROR_CHECK(
        sx127x_set_opmod(SX127X_MODE_TX, SX127X_MODULATION_LORA, device));

    char payload_str[pkt->payload_len + 1];
    memcpy(payload_str, pkt->payload, pkt->payload_len);
    payload_str[pkt->payload_len] = '\0';
    ESP_LOGI(TAG,
             "transmitting packet: recipient: %d | sender: %d | message_id: %d "
             "| payload_len: %d | payload: %s",
             pkt->recipient_id, pkt->sender_id, pkt->message_id,
             pkt->payload_len, payload_str);

    size_t data_length = sizeof(lora_pkt_t) + pkt->payload_len;
    lora_info_t *info =
        (lora_info_t *)malloc(sizeof(lora_info_t) + data_length);
    info->pkt = pkt;
    info->pkt_size = data_length;

    uint8_t *copied_data = (uint8_t *)(info + 1);
    memcpy(copied_data, pkt, data_length);
    info->pkt = (lora_pkt_t *)copied_data;

    event_t *event = create_event(EVENT_TYPE_NOTIFICATION, UI_EVENT_SND_LORA,
                                  info, sizeof(lora_info_t) + data_length);
    if (event) {
        event_dispatcher_post(event);
    }

    free(pkt);
}

void configure_sender(sx127x *device) {
    sx127x_tx_set_callback(tx_callback, device, device);

    ESP_ERROR_CHECK(sx127x_tx_set_pa_config(
        SX127X_PA_PIN_BOOST, supported_power_levels[current_power_level],
        device));
    sx127x_tx_header_t header = {.enable_crc = true,
                                 .coding_rate = SX127X_CR_4_5};

    ESP_ERROR_CHECK(sx127x_lora_tx_set_explicit_header(&header, device));
}
