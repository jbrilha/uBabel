#ifndef LORA_H
#define LORA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct lora_radio lora_radio_t;

typedef struct {
    uint32_t frequency;
    uint32_t offset;
    uint8_t spreading_factor;
    uint8_t bandwidth;
    uint8_t code_rate;
    uint8_t optimization;
    int8_t tx_power;
    uint8_t ramp_time;
} lora_config_t;

typedef struct __attribute__((packed)) {
    uint8_t recipient_id;
    uint8_t sender_id;
    uint16_t message_id;
    uint8_t flags; // may be useful for ACK, priority, etc?
    uint8_t payload_len;
    uint8_t payload[];
} lora_packet_t;

typedef struct {
    int16_t rssi;
    int8_t snr;
    bool crc_err;
    int32_t freq_err;
    lora_packet_t *pkt;
    uint8_t pkt_size;
} lora_info_t;

typedef struct {
    uint8_t *data;
    size_t length;
    int16_t rssi;
    int8_t snr;
} lora_rx_info_t;

struct lora_radio {
    const char *name;

    // hardware-specific (opaque pointer to sx126x or sx127x)
    void *chip;

    bool (*init)(lora_radio_t *r, const lora_config_t *config);
    void (*deinit)(lora_radio_t *r); // TODO

    void (*calibrate_image)(lora_radio_t *r, uint32_t freq);
    void (*clear_IRQ_status)(lora_radio_t *r, uint16_t irq_mask);

    void (*set_mode)(lora_radio_t *r, uint8_t mode);
    void (*set_packet_type)(lora_radio_t *r, uint8_t packet_type);
    void (*set_frequency)(lora_radio_t *r, uint64_t freq, int32_t offset);
    void (*set_tx_params)(lora_radio_t *r, int8_t power, uint8_t ramp_time);
    void (*set_modulation_params)(lora_radio_t *r, uint8_t spreading_factor,
                                  uint8_t bandwidth, uint8_t code_rate,
                                  uint8_t optimization);
    void (*set_buf_base_addr)(lora_radio_t *r, uint8_t tx_base_addr,
                              uint8_t rx_base_addr);
    void (*set_packet_params)(lora_radio_t *r, uint16_t preamble_len,
                              uint8_t fixed_or_variable_len, uint8_t packet_len,
                              uint8_t CRC_mode, uint8_t IQ_mode);
    void (*set_sync_word)(lora_radio_t *r, uint16_t sync_word);
    void (*set_high_sensitivity)(lora_radio_t *r);
    void (*set_DIO_IRQ_params)(lora_radio_t *r, uint16_t irq_mask,
                               uint16_t dio0_mask, uint16_t dio1_mask,
                               uint16_t dio2_mask);
    void (*set_tx)(lora_radio_t *r, uint32_t timeout);
    void (*set_rx)(lora_radio_t *r, uint32_t timeout);

    int (*sleep)(lora_radio_t *r);  // TODO
    int (*wakeup)(lora_radio_t *r); // TODO

    int (*transmit)(lora_radio_t *r, uint8_t *data, size_t len,
                    uint32_t timeout_ms);
    int (*receive)(lora_radio_t *r, uint8_t *rx_buf, size_t max_len,
                   uint32_t timeout_ms);
};

bool lora_init_w_config(lora_radio_t *r, const lora_config_t *config);
bool lora_init(lora_radio_t *r);
void lora_deinit(lora_radio_t *r);

void lora_set_mode(lora_radio_t *r, uint8_t mode);
void lora_set_packet_type(lora_radio_t *r, uint8_t packet_type);
void lora_set_frequency(lora_radio_t *r, uint64_t freq, int32_t offset);
void lora_calibrate_image(lora_radio_t *r, uint32_t freq);
void lora_set_tx_params(lora_radio_t *r, int8_t power, uint8_t ramp_time);
void lora_set_modulation_params(lora_radio_t *r, uint8_t spreading_factor,
                                uint8_t bandwidth, uint8_t code_rate,
                                uint8_t optimization);
void lora_set_buf_base_addr(lora_radio_t *r, uint8_t tx_base_addr,
                            uint8_t rx_base_addr);
void lora_set_packet_params(lora_radio_t *r, uint16_t preamble_len,
                            uint8_t fixed_or_variable_len, uint8_t packet_len,
                            uint8_t CRC_mode, uint8_t IQ_mode);
void lora_set_sync_word(lora_radio_t *r, uint16_t sync_word);
void lora_set_high_sensitivity(lora_radio_t *r);

int lora_transmit_packet(lora_radio_t *r, const lora_packet_t *packet,
                         uint32_t timeout_ms);
int lora_transmit_raw(lora_radio_t *r, uint8_t *data, size_t data_len,
                      uint32_t timeout_ms);

int lora_receive_raw(lora_radio_t *r, uint8_t *rx_buf, size_t max_len,
                     uint32_t timeout_ms);
int lora_receive_packet(lora_radio_t *r, lora_packet_t *packet, size_t max_len,
                        uint32_t timeout_ms);
int lora_receive_info(lora_radio_t *r, lora_rx_info_t *rx_info,
                      uint32_t timeout_ms);

int lora_sleep(lora_radio_t *r);
int lora_wakeup(lora_radio_t *r);

lora_packet_t *new_lora_packet(uint8_t recipient, uint8_t sender,
                               uint16_t msg_id, uint8_t flags,
                               const uint8_t *payload, size_t payload_len);

bool lora_start_sender(lora_radio_t *r);
bool lora_start_receiver(lora_radio_t *r);

#endif // !LORA_H
