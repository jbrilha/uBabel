/*
 * Largely based on this Arduino library
 * https://github.com/StuartsProjects/SX12XX-LoRa
 */

#include "sx127x.h"
#include "gpio_hal.h"
#include "lora.h"
#include "spi_hal.h"

#include "sx127x_defs.h"
#include <math.h>
#include <stdint.h>

// TODO these are from Arduino, move into HAL or something
#define set_bit(value, bit) ((value) |= (1UL << (bit)))
#define read_bit(value, bit) (((value) >> (bit)) & 0x01)

// these are for the M5 Core Basic
#define CS_PIN (5)
#define RST_PIN (26)
#define DIO0_PIN (36)

typedef struct {
    lora_radio_t *radio; // backpointer for consistency with sx126x
    int8_t rst_pin;
    int8_t dio0_pin;
    spi_dev_handle_t spi_dev;
    uint8_t packet_type;
    uint32_t frequency;
    int32_t offset;
    bool use_CRC;
    uint16_t IRQ_MSB;
    uint8_t tx_packet_len; // last packet transmitted
    uint8_t rx_packet_len; // last packet received
} sx127x_t;

static const char *TAG = "SX127x";

static bool sx127x_check_device(sx127x_t *c);
static void sx127x_reset_device(sx127x_t *c);
static void sx127x_set_mode(lora_radio_t *r, uint8_t mode);
static void sx127x_set_packet_type(lora_radio_t *r, uint8_t packet_type);
static void sx127x_set_frequency(lora_radio_t *r, uint64_t freq,
                                 int32_t offset);
static void sx127x_calibrate_image(lora_radio_t *r, uint32_t freq);
static void sx127x_set_tx_params(lora_radio_t *r, int8_t tx_power,
                                 uint8_t ramp_time);
static void sx127x_set_modulation_params(lora_radio_t *r,
                                         uint8_t spreading_factor,
                                         uint8_t bandwidth, uint8_t code_rate,
                                         uint8_t optimization);
static void sx127x_set_buf_base_addr(lora_radio_t *r, uint8_t tx_base_addr,
                                     uint8_t rx_base_addr);
static void sx127x_set_packet_params(lora_radio_t *r, uint16_t preamble_len,
                                     uint8_t fixed_or_variable_len,
                                     uint8_t packet_len, uint8_t CRC_mode,
                                     uint8_t IQ_mode);
static void sx127x_set_sync_word(lora_radio_t *r, uint16_t sync_word);
static void sx127x_set_high_sensitivity(lora_radio_t *r);
static void sx127x_set_DIO_IRQ_params(lora_radio_t *r, uint16_t irq_mask,
                                      uint16_t dio0_mask, uint16_t dio1_mask,
                                      uint16_t dio2_mask);
static void sx127x_clear_IRQ_status(lora_radio_t *r, uint16_t irq_mask);
static void sx127x_set_tx(lora_radio_t *r, uint32_t timeout);
static void sx127x_set_rx(lora_radio_t *r, uint32_t timeout);
static int sx127x_read_IRQ_status(lora_radio_t *r);
static int sx127x_transmit(lora_radio_t *r, uint8_t *data, size_t len,
                           uint32_t timeout_ms);
static int sx127x_receive(lora_radio_t *r, uint8_t *rx_buf, size_t max_len,
                          uint32_t timeout_ms);
static bool sx127x_driver_init(lora_radio_t *r, const lora_config_t *config);

// utils at the bottom
static uint32_t get_actual_bandwidth(uint8_t bw_reg_value);
static float calc_symbol_ms(float bw, uint8_t sf);
static uint8_t calc_optimization(uint8_t bw, uint8_t sf);

static void write_register(sx127x_t *c, uint8_t address, uint8_t value) {
    uint8_t masked_addr = address | 0x80;
    if (!spi_hal_write_register(c->spi_dev, masked_addr, &value, 1)) {
        ESP_LOGE(TAG, "FAILED TO TRANSMIT SPI VAL %d TO ADDR %d", value,
                 address);
    }
}

static uint8_t read_register(sx127x_t *c, uint8_t address) {
    uint8_t data;

    uint8_t masked_addr = address & 0x7F;
    if (!spi_hal_read_register(c->spi_dev, masked_addr, &data, 1)) {
        ESP_LOGE(TAG, "FAILED TO READ SPI VAL FROM ADDR %d", address);
        return 0;
    }

    return data;
}

// TODO might be worth distinguishing with different models
static bool rx_done(sx127x_t *c) { return gpio_get_level(c->dio0_pin); }
static bool tx_done(sx127x_t *c) { return gpio_get_level(c->dio0_pin); }

static sx127x_t *sx127x_chip_init_on_pins(lora_radio_t *r, int8_t cs_pin,
                                          int8_t rst_pin, int8_t dio0_pin) {
    sx127x_t *c = malloc(sizeof(sx127x_t));
    if (!c) {
        return NULL;
    }

    r->chip = c;
    c->radio = r;

    c->rst_pin = rst_pin;
    c->dio0_pin = dio0_pin;

    spi_dev_cfg_t cfg = spi_hal_create_config(cs_pin, 8E6, 0);
    c->spi_dev = spi_hal_add_device(&cfg);
    if (!c->spi_dev) {
        r->chip = NULL;
        free(c);
        return NULL;
    }

    gpio_init_pin(c->rst_pin);
    gpio_set_pin_dir(c->rst_pin, GPIO_OUTPUT);
    gpio_set_pin_level(c->rst_pin, 0);

    gpio_init_pin(c->dio0_pin);
    gpio_set_pin_dir(c->dio0_pin, GPIO_INPUT);

    sx127x_reset_device(c);

    if (!sx127x_check_device(c)) {
        LOG_ERROR(TAG, "failed to initialize lora chip");
        spi_hal_remove_device(c->spi_dev);
        r->chip = NULL;
        free(c);
        return NULL;
    }

    return c;
}

static sx127x_t *sx127x_chip_init(lora_radio_t *r) {
    return sx127x_chip_init_on_pins(r, CS_PIN, RST_PIN, DIO0_PIN);
}

static void sx127x_set_mode(lora_radio_t *r, uint8_t mode) {
    sx127x_t *c = (sx127x_t *)r->chip;
    write_register(c, REG_OPMODE, mode + c->packet_type);
}

static bool sx127x_check_device(sx127x_t *c) {

    uint8_t reg_data1, reg_data2;
    reg_data1 = read_register(c, REG_FRMID); // frequency setting low byte
    write_register(c, REG_FRMID, (reg_data1 + 1));

    reg_data2 = read_register(c, REG_FRMID); // read changed value back
    write_register(c, REG_FRMID, reg_data1); // restore to original value

    return (reg_data2 == (reg_data1 + 1));
}

static void sx127x_reset_device(sx127x_t *c) {
    gpio_set_pin_level(c->rst_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(2));
    gpio_set_pin_level(c->rst_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
}

static void sx127x_set_packet_type(lora_radio_t *r, uint8_t packet_type) {
    sx127x_t *c = (sx127x_t *)r->chip;
    c->packet_type = packet_type;

    uint8_t reg_data = read_register(c, REG_OPMODE) & 0x7F;

    if (packet_type == PACKET_TYPE_LORA) {
        // need to set sleep mode before setting lora mode
        write_register(c, REG_OPMODE, 0x80);
        // back to original standby mode
        write_register(c, REG_OPMODE, (reg_data + 0x80));
    } else {
        LOG_ERROR(TAG, "packet type not supported yet");
    }
}

static void sx127x_set_frequency(lora_radio_t *r, uint64_t freq,
                                 int32_t offset) {
    sx127x_t *c = (sx127x_t *)r->chip;

    c->frequency = freq;
    c->offset = offset;

    uint64_t freq64 = freq + offset;
    freq64 = ((uint64_t)freq64 << 19) / 32000000;

    uint8_t freqregH = freq64 >> 16;
    uint8_t freqregM = freq64 >> 8;
    uint8_t freqregL = freq64;
    write_register(c, REG_FRMSB, freqregH);
    write_register(c, REG_FRMID, freqregM);
    write_register(c, REG_FRLSB, freqregL);
}

static void sx127x_calibrate_image(lora_radio_t *r, uint32_t freq) {
    sx127x_t *c = (sx127x_t *)r->chip;

    uint8_t reg_data, saved_mode;

    saved_mode = read_register(c, REG_OPMODE);
    write_register(c, REG_OPMODE, 0x00); // sleep
    write_register(c, REG_OPMODE, 0x00); // sleep
    write_register(c, REG_OPMODE, 0x01); // standby FSK mode

    reg_data = (read_register(c, REG_IMAGECAL) | 0x40);
    write_register(c, REG_IMAGECAL, reg_data); // start calibration

    // calibration time is 10ms, add some leeway
    vTaskDelay(pdMS_TO_TICKS(15));

    write_register(c, REG_OPMODE, 0x00); // sleep
    write_register(c, REG_OPMODE, saved_mode & 0xFE);
    write_register(c, REG_OPMODE, saved_mode);
}

static void sx127x_set_tx_params(lora_radio_t *r, int8_t tx_power,
                                 uint8_t ramp_time) {
    sx127x_t *c = (sx127x_t *)r->chip;
    uint8_t max_power, output_power, OCP_trim, boost_val;

    boost_val = PABOOSTON;
    max_power = MAXPOWER17dBm;

    // upper power limit for SX127X
    if (tx_power > 20) {
        tx_power = 20;
    }
    if (tx_power < 2) {
        tx_power = 2;
    }

    if (tx_power > 17) {
        write_register(c, REG_PADAC, 0x87);
        // power range is 15dBm, max is now 20dBm min is 5dBm
        output_power = tx_power - 5;
    } else {
        write_register(c, REG_PADAC, 0x84);
        // power range is 15dBm, max is 17dBm min is 2dBm
        output_power = tx_power - 2;
    }

    OCP_trim = OCP_TRIM_110MA; // value for OcpTrim 11dBm to 16dBm

    if (tx_power >= 17) {
        OCP_trim = OCP_TRIM_150MA;
    }

    if (tx_power <= 10) {
        OCP_trim = OCP_TRIM_80MA;
    }

    write_register(c, REG_PARAMP, ramp_time);
    write_register(c, REG_OCP, (OCP_trim + 0x20));
    write_register(c, REG_PACONFIG, (boost_val + max_power + output_power));
}

static void sx127x_set_modulation_params(lora_radio_t *r,
                                         uint8_t spreading_factor,
                                         uint8_t bandwidth, uint8_t code_rate,
                                         uint8_t optimization) {
    sx127x_t *c = (sx127x_t *)r->chip;
    uint8_t reg_data;

    reg_data = (read_register(c, REG_MODEMCONFIG2) & (~READ_SF_AND_X));
    write_register(c, REG_MODEMCONFIG2, (reg_data + (spreading_factor << 4)));

    reg_data = (read_register(c, REG_MODEMCONFIG1) & (~READ_BW_AND_X));
    write_register(c, REG_MODEMCONFIG1, (reg_data + bandwidth));

    reg_data = (read_register(c, REG_MODEMCONFIG1) & (~READ_CR_AND_X));
    write_register(c, REG_MODEMCONFIG1, (reg_data + (code_rate)));

    if (optimization == LDRO_AUTO) {
        optimization = calc_optimization(bandwidth, spreading_factor);
    }

    reg_data = (read_register(c, REG_MODEMCONFIG3) & (~READ_LDRO_AND_X));
    write_register(c, REG_MODEMCONFIG3, (reg_data + (optimization << 3)));

    int32_t updated_offset = c->offset;

    // optimizations present in SX1276_77_8_ErrataNote_1_1

    // ERRATA 2.3 - Receiver Spurious Reception of a LoRa Signal
    if (bandwidth < LORA_BW_500) {
        write_register(c, REG_DETECTOPTIMIZE,
                       read_register(c, REG_DETECTOPTIMIZE) & 0x7F);
        write_register(c, REG_LRTEST30, 0x00);

        switch (bandwidth) {
        case LORA_BW_007: // 7.8 kHz
            write_register(c, REG_LRTEST2F, 0x48);
            updated_offset = c->offset + 7800;
            break;
        case LORA_BW_010: // 10.4 kHz
            write_register(c, REG_LRTEST2F, 0x44);
            updated_offset = c->offset + 10400;
            break;
        case LORA_BW_015: // 15.6 kHz
            write_register(c, REG_LRTEST2F, 0x44);
            updated_offset = c->offset + 15600;
            break;
        case LORA_BW_020: // 20.8 kHz
            write_register(c, REG_LRTEST2F, 0x44);
            updated_offset = c->offset + 20800;
            break;
        case LORA_BW_031: // 31.2 kHz
            write_register(c, REG_LRTEST2F, 0x44);
            updated_offset = c->offset + 31200;
            break;
        case LORA_BW_041: // 41.4 kHz
            write_register(c, REG_LRTEST2F, 0x44);
            updated_offset = c->offset + 41400;
            break;
        case LORA_BW_062: // 62.5 kHz
            write_register(c, REG_LRTEST2F, 0x40);
            break;
        case LORA_BW_125: // 125 kHz
            write_register(c, REG_LRTEST2F, 0x40);
            break;
        case LORA_BW_250: // 250 kHz
            write_register(c, REG_LRTEST2F, 0x40);
            break;
        }
    } else {
        write_register(c, REG_DETECTOPTIMIZE,
                       read_register(c, REG_DETECTOPTIMIZE) | 0x80);
    }

    sx127x_set_frequency(r, c->frequency, updated_offset); // update frequency

    // ERRATA 2.1 - SX1276_77_8_ErrataNote_1_1
    if ((bandwidth == LORA_BW_500) && (c->frequency >= 862000000)) {
        // ERRATA 2.1 - Sensitivity Optimization with a 500 kHz Bandwidth >=
        // 862Mhz
        write_register(c, REG_HIGHBWOPTIMIZE1, 0x02);
        write_register(c, REG_HIGHBWOPTIMIZE2, 0x64);
    } else if (bandwidth == LORA_BW_500) {
        // ERRATA 2.1 - Sensitivity Optimization with a 500 kHz Bandwidth 410Mhz
        // to 525Mhz
        write_register(c, REG_HIGHBWOPTIMIZE1, 0x02);
        write_register(c, REG_HIGHBWOPTIMIZE2, 0x7F);
    } else {
        // ERRATA 2.1 - Sensitivity Optimization with a 500 kHz Bandwidth
        write_register(c, REG_HIGHBWOPTIMIZE1, 0x03);
    }

    // Datasheet SX1276-7-8-9_May_2020, page 115, REG_DETECTOPTIMIZE = 0x31,
    // REG_LRDETECTIONTHRESHOLD = 0x37
    if (spreading_factor == LORA_SF6) {
        write_register(c, REG_DETECTOPTIMIZE,
                       (read_register(c, REG_DETECTOPTIMIZE) & 0xF8) | 0x05);
        write_register(c, REG_DETECTIONTHRESHOLD, 0x0C);
    } else {
        write_register(c, REG_DETECTOPTIMIZE,
                       (read_register(c, REG_DETECTOPTIMIZE) & 0xF8) | 0x03);
        write_register(c, REG_DETECTIONTHRESHOLD, 0x0A);
    }
}

static void sx127x_set_buf_base_addr(lora_radio_t *r, uint8_t tx_base_addr,
                                     uint8_t rx_base_addr) {
    sx127x_t *c = (sx127x_t *)r->chip;

    write_register(c, REG_FIFOTXBASEADDR, tx_base_addr);
    write_register(c, REG_FIFORXBASEADDR, rx_base_addr);
}

static void sx127x_set_packet_params(lora_radio_t *r, uint16_t preamble_len,
                                     uint8_t fixed_or_variable_len,
                                     uint8_t packet_len, uint8_t CRC_mode,
                                     uint8_t IQ_mode) {

    sx127x_t *c = (sx127x_t *)r->chip;
    uint8_t preamble_MSB, preamble_LSB, reg_data;
    preamble_MSB = preamble_len >> 8;
    preamble_LSB = preamble_len & 0xFF;

    // Preamble length reg 0x20, 0x21
    write_register(c, REG_PREAMBLEMSB, preamble_MSB);
    write_register(c, REG_PREAMBLELSB, preamble_LSB);

    // TX Packet length reg 0x22
    write_register(c, REG_PAYLOADLENGTH, packet_len);

    // IQ mode reg 0x33 and 0x3B
    if (IQ_mode == LORA_IQ_INVERTED) {
        write_register(c, REG_INVERTIQ, 0x66);
        write_register(c, REG_INVERTIQ2, 0x19);
    }

    if (IQ_mode == LORA_IQ_NORMAL) {
        write_register(c, REG_INVERTIQ, 0x27);
        write_register(c, REG_INVERTIQ2, 0x1d);
    }

    c->use_CRC = CRC_mode;

    // Fixed\Variable length packets
    // mask off bit 0
    reg_data = ((read_register(c, REG_MODEMCONFIG1)) & (~READ_IMPLCIT_AND_X));
    // write out with bit 0 set appropriatly
    write_register(c, REG_MODEMCONFIG1, (reg_data + fixed_or_variable_len));

    // CRC on payload
    // mask off all bits bar CRC on - bit 2
    reg_data = ((read_register(c, REG_MODEMCONFIG2)) & (~READ_HASCRC_AND_X));
    // write out with CRC bit 2 set appropriatly
    write_register(c, REG_MODEMCONFIG2, (reg_data + (CRC_mode << 2)));
}

static void sx127x_set_sync_word(lora_radio_t *r, uint16_t sync_word) {
    sx127x_t *c = (sx127x_t *)r->chip;

    write_register(c, REG_SYNCWORD, sync_word);
}

static void sx127x_set_high_sensitivity(lora_radio_t *r) {
    sx127x_t *c = (sx127x_t *)r->chip;

    // MAX gain for PA_BOOST and for RFO_HF set 150% LNA current
    write_register(c, REG_LNA, 0x23);
}

static void sx127x_set_DIO_IRQ_params(lora_radio_t *r, uint16_t irq_mask,
                                      uint16_t dio0_mask, uint16_t dio1_mask,
                                      uint16_t dio2_mask) {
    sx127x_t *c = (sx127x_t *)r->chip;
    uint8_t mask0, mask1, mask2;

    switch (dio0_mask) {
    case IRQ_RX_DONE:
        mask0 = 0;
        break;
    case IRQ_TX_DONE:
        mask0 = 0x40;
        break;
    case IRQ_CAD_DONE:
        mask0 = 0x80;
        break;
    default:
        mask0 = 0x0C;
    }

    switch (dio1_mask) {
    case IRQ_RX_TIMEOUT:
        mask1 = 0;
        break;

    case IRQ_FSHS_CHANGE_CHANNEL:
        mask1 = 0x10;
        break;

    case IRQ_CAD_ACTIVITY_DETECTED:
        mask1 = 0x20;
        break;

    default:
        mask1 = 0x30;
    }

    mask2 = 0x00;

    write_register(c, REG_IRQFLAGSMASK, ~irq_mask);
    write_register(c, REG_DIOMAPPING1, (mask0 + mask1 + mask2));
}

static void sx127x_clear_IRQ_status(lora_radio_t *r, uint16_t irq_mask) {

    sx127x_t *c = (sx127x_t *)r->chip;
    uint8_t mask_LSB;
    uint16_t mask_MSB;
    // make sure c->IRQ_MSB does not have LSB set
    c->IRQ_MSB = c->IRQ_MSB & 0xFF00;
    mask_LSB = (irq_mask & 0xFF);
    mask_MSB = (irq_mask & 0xFF00);
    write_register(c, REG_IRQFLAGS, mask_LSB); // clear standard IRQs
    c->IRQ_MSB = (c->IRQ_MSB & (~mask_MSB));   // only want top bits set
}

static void sx127x_set_tx(lora_radio_t *r, uint32_t timeout) {
    sx127x_t *c = (sx127x_t *)r->chip;
    // no TX timeout function for SX127X!! timeout is ignored

    sx127x_clear_IRQ_status(r, IRQ_RADIO_ALL);

    write_register(c, REG_OPMODE, (MODE_TX + 0x88)); // TX on LoRa mode
}

static void sx127x_set_rx(lora_radio_t *r, uint32_t timeout) {
    sx127x_t *c = (sx127x_t *)r->chip;
    // no TX timeout function for SX127X!! timeout is ignored

    sx127x_clear_IRQ_status(r, IRQ_RADIO_ALL);

    write_register(c, REG_OPMODE,
                   (MODE_RXCONTINUOUS + 0x80)); // RX on LoRa mode
}

static int sx127x_read_IRQ_status(lora_radio_t *r) {
    sx127x_t *c = (sx127x_t *)r->chip;
    bool has_CRC;
    uint8_t regdata = read_register(c, REG_IRQFLAGS);
    has_CRC = read_register(c, REG_HOPCHANNEL) & 0x40;

    if (read_bit(regdata, 6)) {
        if (!has_CRC && c->use_CRC) {
            set_bit(c->IRQ_MSB, 10);
        }
    }

    return regdata + c->IRQ_MSB;
}

static int sx127x_transmit(lora_radio_t *r, uint8_t *data, size_t len,
                           uint32_t timeout_ms) {
    sx127x_t *c = (sx127x_t *)r->chip;

    uint8_t ptr;
    uint32_t start_ms;

    if (len == 0) {
        return 0;
    }

    sx127x_set_mode(r, MODE_STDBY_RC);
    // retrieve the TXbase address pointer
    ptr = read_register(c, REG_FIFOTXBASEADDR);
    // and save in FIFO access ptr
    write_register(c, REG_FIFOADDRPTR, ptr);

    if (!spi_hal_write_reg_buffer(c->spi_dev, WREG_FIFO, data, len)) {
        ESP_LOGE(TAG, "FAILED TO TRANSMIT SPI");
        return 0;
    }

    c->tx_packet_len = len;
    write_register(c, REG_PAYLOADLENGTH, c->tx_packet_len);
    sx127x_set_DIO_IRQ_params(r, IRQ_RADIO_ALL, IRQ_TX_DONE, 0, 0);
    sx127x_set_tx(r, 0);

    if (timeout_ms == 0) {
        while (!tx_done(c)) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    } else {
        start_ms = hal_millis();
        while (!tx_done(c) && ((hal_millis() - start_ms) < timeout_ms)) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    sx127x_set_mode(r, MODE_STDBY_RC); // ensure we leave function with TX off

    if (!tx_done(c)) {
        c->IRQ_MSB = IRQ_TX_TIMEOUT;
        return 0;
    }

    return c->tx_packet_len;
}

static int sx127x_receive(lora_radio_t *r, uint8_t *rx_buf, size_t max_len,
                          uint32_t timeout_ms) {
    sx127x_t *c = (sx127x_t *)r->chip;

    uint32_t start_ms;
    uint8_t reg_data;

    sx127x_set_mode(r, MODE_STDBY_RC);

    // retrieve the RXbase address pointer
    reg_data = read_register(c, REG_FIFORXBASEADDR);
    // and save in FIFO access ptr
    write_register(c, REG_FIFOADDRPTR, reg_data);

    sx127x_set_DIO_IRQ_params(r, IRQ_RADIO_ALL, (IRQ_RX_DONE), 0, 0);
    sx127x_set_rx(r, 0);

    if (timeout_ms == 0) {
        while (!rx_done(c)) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    } else {
        start_ms = hal_millis();
        while (!rx_done(c) && ((hal_millis() - start_ms) < timeout_ms)) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    sx127x_set_mode(r, MODE_STDBY_RC);
    if (!rx_done(c)) {
        c->IRQ_MSB = IRQ_RX_TIMEOUT;
        return 0;
    }

    if (sx127x_read_IRQ_status(r) != (IRQ_RX_DONE + IRQ_HEADER_VALID)) {
        return 0; // no RX done and header valid only, could be CRC error
    }

    c->rx_packet_len = read_register(c, REG_RXNBBYTES);
    if (c->rx_packet_len > max_len) {
        c->rx_packet_len = max_len;
    }

    if (!spi_hal_read_reg_buffer(c->spi_dev, REG_FIFO, rx_buf,
                                 c->rx_packet_len)) {
        ESP_LOGE(TAG, "FAILED TO TRANSMIT SPI");
        return 0;
    }

    return c->rx_packet_len;
}

static bool sx127x_driver_init(lora_radio_t *r, const lora_config_t *config) {
    sx127x_set_mode(r, MODE_STDBY_RC);
    // others not yet supported!! here for future implementations
    sx127x_set_packet_type(r, PACKET_TYPE_LORA);
    sx127x_set_frequency(r, config->frequency, config->offset);
    // calibration is independent of frequency in the sx127x family
    sx127x_calibrate_image(r, 0);
    sx127x_set_modulation_params(r, LORA_SF7, LORA_BW_125, LORA_CR_4_5,
                                 LDRO_AUTO);
    sx127x_set_buf_base_addr(r, 0x00, 0x00);
    sx127x_set_packet_params(r, 8, LORA_PACKET_VARIABLE_LENGTH, 255,
                             LORA_CRC_ON, LORA_IQ_NORMAL);
    sx127x_set_tx_params(r, config->tx_power, RADIO_RAMP_DEFAULT);
    sx127x_set_sync_word(r, LORA_MAC_PRIVATE_SYNCWORD);
    sx127x_set_high_sensitivity(r);

    return true;
}

lora_radio_t *lora_create_sx127x_radio(void) {
    lora_radio_t *radio = malloc(sizeof(lora_radio_t));
    if (!radio)
        return NULL;

    sx127x_t *c = sx127x_chip_init(radio);
    if (!c) {
        free(radio);
        return NULL;
    }

    radio->name = TAG;
    // radio->chip = c; // done internally in chip init
    radio->init = sx127x_driver_init;
    radio->calibrate_image = sx127x_calibrate_image;
    radio->clear_IRQ_status = sx127x_clear_IRQ_status;
    radio->read_IRQ_status = sx127x_read_IRQ_status;
    // TODO might need to restructure these to not be called in the chip init
    // radio->check_device = sx127x_check_device;
    // radio->reset_device = sx127x_reset_device;

    radio->set_mode = sx127x_set_mode;
    radio->set_packet_type = sx127x_set_packet_type;
    radio->set_frequency = sx127x_set_frequency;
    radio->set_tx_params = sx127x_set_tx_params;
    radio->set_modulation_params = sx127x_set_modulation_params;
    radio->set_buf_base_addr = sx127x_set_buf_base_addr;
    radio->set_packet_params = sx127x_set_packet_params;
    radio->set_sync_word = sx127x_set_sync_word;
    radio->set_high_sensitivity = sx127x_set_high_sensitivity;
    radio->set_DIO_IRQ_params = sx127x_set_DIO_IRQ_params;
    radio->set_tx = sx127x_set_tx;
    radio->set_rx = sx127x_set_rx;

    radio->transmit = sx127x_transmit;
    radio->receive = sx127x_receive;

    return radio;
}

static uint8_t calc_optimization(uint8_t bw, uint8_t sf) {
    uint32_t temp_bw;
    float symbol_time;

    temp_bw = get_actual_bandwidth(bw);
    symbol_time = calc_symbol_ms(temp_bw, sf);

    if (symbol_time > 16) {
        return LDRO_ON;
    } else {
        return LDRO_OFF;
    }
}

static float calc_symbol_ms(float bw, uint8_t sf) {
    float ms;
    ms = (bw / pow(2, sf));
    ms = (1000 / ms);
    return ms;
}

static uint32_t get_actual_bandwidth(uint8_t bw_reg_value) {
    switch (bw_reg_value) {
    case 0:
        return 7800;
    case 16:
        return 10400;
    case 32:
        return 15600;
    case 48:
        return 20800;
    case 64:
        return 31200;
    case 80:
        return 41700;
    case 96:
        return 62500;
    case 112:
        return 125000;
    case 128:
        return 250000;
    case 144:
        return 500000;
    default:
        return 0xFF;
    }
}
