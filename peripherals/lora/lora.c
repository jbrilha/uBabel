#include "lora.h"
#include "platform.h"

#include "event_dispatcher.h"
#include "lora_events.h"
#include <string.h>

#define LORA_MAX_PKT_LENGTH 255
#define LORA_HEADER_SIZE sizeof(lora_packet_t)
#define LORA_MAX_PAYLOAD_SIZE (LORA_MAX_PKT_LENGTH - LORA_HEADER_SIZE)

#define LORA_BROADCAST_ID 0xFF

static const char *TAG = "LoRa Driver";

lora_packet_t *new_lora_packet(uint8_t recipient, uint8_t sender,
                               uint16_t msg_id, uint8_t flags,
                               const uint8_t *payload, size_t payload_len) {
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

int lora_transmit_raw(lora_radio_t *r, uint8_t *data, size_t data_len,
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

int lora_transmit_packet(lora_radio_t *r, const lora_packet_t *packet,
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

    uint8_t tx_len = r->transmit(r, (uint8_t *)packet, total_len, timeout_ms);

    return tx_len;
}

int lora_receive_raw(lora_radio_t *r, uint8_t *rx_buf, size_t max_len,
                     uint32_t timeout_ms) {
    if (!r || !r->receive || !rx_buf) {
        return 0;
    }

    if (max_len > LORA_MAX_PKT_LENGTH) {
        LOG_ERROR(TAG, "invalid packet size %d", max_len);
        return 0;
    }

    return r->receive(r, rx_buf, max_len, timeout_ms);
}

int lora_receive_packet(lora_radio_t *r, lora_packet_t *packet, size_t max_len,
                        uint32_t timeout_ms) {
    if (!r || !r->receive || !packet) {
        return 0;
    }

    if (max_len > LORA_MAX_PKT_LENGTH) {
        LOG_ERROR(TAG, "invalid packet size %d", max_len);
        return 0;
    }

    uint8_t buf[max_len];
    int rx_len = r->receive(r, buf, max_len, 1000);
    if (rx_len <= 0) {
        LOG_ERROR(TAG, "Failed to receive packet, len: %d", rx_len);
        return 0;
    }

    if (rx_len < sizeof(lora_packet_t)) {
        LOG_WARN(TAG, "Packet too small: %d bytes", rx_len);
        return 0;
    }

    memcpy(packet, buf, rx_len);

    if (rx_len < sizeof(lora_packet_t) + packet->payload_len) {
        LOG_WARN(TAG, "Truncated payload");
    }

    return rx_len;
}

static void lora_sender_task(void *pvParameters) {
    uint8_t i = 0;
    lora_radio_t *r = (lora_radio_t *)pvParameters;
    if (!r) {
        vTaskDelete(NULL);
    }

    char msg[32];
    uint8_t tx_len = 0;
    while (i++ < 255) {
        snprintf(msg, sizeof(msg), "HELLO WORLD %d", i);
        lora_packet_t *pkt =
            new_lora_packet(0xFF, 0xAB, i, 0x00, (const uint8_t *)msg, sizeof(msg));
        if ((tx_len = lora_transmit_packet(r, pkt, 1000))) {
            // LOG_WARN(TAG, "SENT");
            // LOG_INFO(TAG, "Packet sent:");
            // LOG_INFO(TAG, "  recipient_id: 0x%02X", pkt->recipient_id);
            // LOG_INFO(TAG, "  sender_id   : 0x%02X", pkt->sender_id);
            // LOG_INFO(TAG, "  message_id  : %u", pkt->message_id);
            // LOG_INFO(TAG, "  flags       : 0x%02X", pkt->flags);
            // LOG_INFO(TAG, "  payload_len : %u", pkt->payload_len);
            // LOG_INFO(TAG, "  payload str : %.*s", pkt->payload_len,
            //          pkt->payload);

            event_t *event = create_event(EVENT_TYPE_NOTIFICATION,
                                          UI_EVENT_SND_LORA, pkt, tx_len);

            if (event) {
                event_dispatcher_post(event);
            } else {
                free(pkt);
            }
        } else {
            if (pkt) {
                free(pkt);
            }
            LOG_ERROR(TAG, "FAILED TO SEND");
        }

        tx_len = 0;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    LOG_INFO(TAG, "DONE");

    vTaskDelete(NULL);
}

/* static void lora_sender_task(void *pvParameters) {
    uint8_t i = 0;
    lora_radio_t *r = (lora_radio_t *)pvParameters;
    if (!r) {
        vTaskDelete(NULL);
    }

    uint8_t msg[] = "Hello World 1234567890*";
    uint8_t tx_len = 0;
    while (true && i++ < 255) {
        if ((tx_len = lora_transmit_raw(r, msg, sizeof(msg), 1000))) {
            LOG_WARN(TAG, "SENT %.*s", tx_len, msg);
        } else {
            LOG_ERROR(TAG, "FAILED TO SEND");
        }

        tx_len = 0;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    LOG_INFO(TAG, "DONE");

    vTaskDelete(NULL);
} */

bool lora_start_sender(lora_radio_t *r) {
    return xTaskCreate(lora_sender_task, "LORA_SEND_TASK", 4096, (void *)r, 2,
                       NULL) == pdPASS;
}

static void lora_receiver_task(void *pvParameters) {
    lora_radio_t *r = (lora_radio_t *)pvParameters;
    if (!r) {
        vTaskDelete(NULL);
    }

    uint8_t pkt_buf[LORA_MAX_PKT_LENGTH];
    lora_packet_t *pkt = (lora_packet_t *)pkt_buf;

    while (true) {
        if (lora_receive_packet(r, pkt, LORA_MAX_PKT_LENGTH, 1000)) {
            LOG_INFO(TAG, "Packet received:");
            LOG_INFO(TAG, "  recipient_id: 0x%02X", pkt->recipient_id);
            LOG_INFO(TAG, "  sender_id   : 0x%02X", pkt->sender_id);
            LOG_INFO(TAG, "  message_id  : %u", pkt->message_id);
            LOG_INFO(TAG, "  flags       : 0x%02X", pkt->flags);
            LOG_INFO(TAG, "  payload_len : %u", pkt->payload_len);

            LOG_INFO(TAG, "  payload str : %.*s", pkt->payload_len,
                     pkt->payload);
        } else {
            LOG_ERROR(TAG, "No packet received");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete(NULL);
}

/* static void lora_receiver_task(void *pvParameters) {
    lora_radio_t *r = (lora_radio_t *)pvParameters;
    if (!r) {
        vTaskDelete(NULL);
    }
    uint8_t rx_buf[64];
    int len = 0;
    while (true) {
        if ((len = lora_receive_raw(r, rx_buf, LORA_MAX_PKT_LENGTH, 1000))) {
            LOG_INFO(TAG, "RX %d bytes: %.*s", len, len, rx_buf);
        } else {
            LOG_ERROR(TAG, "No packet received");
        }

        len = 0;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete(NULL);
} */

bool lora_start_receiver(lora_radio_t *r) {
    return xTaskCreate(lora_receiver_task, "LORA_REC_TASK", 4096, (void *)r, 2,
                       NULL) == pdPASS;
}
