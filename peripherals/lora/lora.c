#include "lora.h"
#include "platform.h"

#include "event_dispatcher.h"
#include "lora_events.h"

#define LORA_MAX_PKT_LENGTH 255
#define LORA_HEADER_SIZE sizeof(lora_packet_t)
#define LORA_MAX_PAYLOAD_SIZE (LORA_MAX_PKT_LENGTH - LORA_HEADER_SIZE)

#define LORA_BROADCAST_ID 0xFF

static const char *TAG = "LoRa Driver";

lora_packet_t *new_lora_packet(uint8_t recipient, uint8_t sender,
                               uint16_t msg_id, uint8_t flags,
                               const uint8_t *payload, uint8_t payload_len) {
    if (payload_len > LORA_MAX_PAYLOAD_SIZE) {
        LOG_ERROR(TAG, "payload exceeds max size: %d out of %d bytes",
                  payload_len, LORA_MAX_PAYLOAD_SIZE);
        return NULL;
    }

    size_t total_size = sizeof(lora_packet_t) + payload_len;

    lora_packet_t *pkt = (lora_packet_t *)malloc(total_size);
    pkt->recipient_id = recipient;
    pkt->sender_id = sender;
    pkt->message_id = msg_id;
    pkt->payload_len = payload_len;
    if (payload && payload_len > 0) {
        memcpy(pkt->payload, payload, payload_len);
    }

    return pkt;
}

bool lora_init_w_config(lora_radio_t *r, const lora_config_t *config) {
    if (!r || !r->init) {
        return false;
    }

    return r->init(r, config);
}

bool lora_init(lora_radio_t *r) {
    if (!r || !r->init) {
        return false;
    }

    lora_config_t config = {
        .frequency = 868000000,
        .offset = 0,
        .tx_power = 10,
    };

    return r->init(r, &config);
}

int lora_transmit_raw(lora_radio_t *r, const uint8_t *data, size_t data_len,
                      uint32_t timeout_ms) {
    if (!r || !r->transmit || !data) {
        return 0;
    }

    if (data_len > LORA_MAX_PKT_LENGTH) {
        LOG_ERROR(TAG, "invalid packet size %d", data_len);
        return 0;
    }

    return r->transmit(r, data, data_len, timeout_ms);
}

int lora_transmit(lora_radio_t *r, const lora_packet_t *packet,
                  uint32_t timeout_ms) {
    if (!r || !r->transmit || !packet) {
        return 0;
    }

    size_t total_len = sizeof(lora_packet_t) + packet->payload_len;

    // shouldn't happen if the packet is created with the helper func but
    // defensive programming and whatnot
    if (total_len > LORA_MAX_PKT_LENGTH) {
        LOG_ERROR(TAG, "invalid packet size %d", total_len);
        return 0;
    }

    uint8_t trans_len =
        r->transmit(r, (const uint8_t *)packet, (uint8_t)total_len, timeout_ms);

    if (trans_len > 0) {
        lora_packet_t *pkt_copy = malloc(total_len);
        if (pkt_copy) {
            memcpy(pkt_copy, packet, total_len);

            event_t *event =
                create_event(EVENT_TYPE_NOTIFICATION, UI_EVENT_SND_LORA,
                             pkt_copy, total_len);

            if (event) {
                event_dispatcher_post(event);
            } else {
                free(pkt_copy);
            }
        }
    }

    return trans_len;
}

static void lora_sender_task(void *pvParameters) {
    uint8_t i = 0;
    lora_radio_t *r = (lora_radio_t *)pvParameters;
    if (r) {
        uint8_t msg[] = "HELLO WORLD";
        while (true && i++ < 255) {
            lora_packet_t *pkt =
                new_lora_packet(0xFF, 0xAB, i, 0x00, msg, sizeof(msg));
            if (lora_transmit(r, pkt, 1000)) {
                ESP_LOGI(TAG, "SENT");
            } else {
                ESP_LOGE(TAG, "FAILED TO SEND");
            }

            free(pkt);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    ESP_LOGI(TAG, "DONE");

    vTaskDelete(NULL);
}

bool lora_start_sender(lora_radio_t *r) {
    return xTaskCreate(lora_sender_task, "LORA_SEND_TASK", 4096, (void *)r, 2,
                       NULL) == pdPASS;
}

static void lora_receiver_task(void *pvParameters) {
    lora_radio_t *r = (lora_radio_t *)pvParameters;
    if (r) {
        // TODO
    }

    vTaskDelete(NULL);
}

bool lora_start_receiver(lora_radio_t *r) {
    return xTaskCreate(lora_receiver_task, "LORA_REC_TASK", 4096, (void *)r, 2,
                       NULL) == pdPASS;
}
