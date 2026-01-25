#include "sx126x.h"
#include "gpio_hal.h"
#include "lora.h"
#include "spi_hal.h"

#include "sx126x_defs.h"
#include <math.h>

#if CONFIG_IDF_TARGET_ESP32S3
#define CS_PIN (8)
#define RST_PIN (2)
#define BUSY_PIN (13)
#define DIO1_PIN (9)
#define RXEN_PIN (11)
#define TXEN_PIN (12)
#elif CONFIG_IDF_TARGET_ESP32C6
#define CS_PIN (0)
#define RST_PIN (1)
#define BUSY_PIN (20)
#define DIO1_PIN (14)
#define RXEN_PIN (18)
#define TXEN_PIN (19)
#else
// these should be tested first!!
#define CS_PIN (9)
#define RST_PIN (1)
#define BUSY_PIN (20)
#define DIO1_PIN (14)
#define RXEN_PIN (18)
#define TXEN_PIN (19)
#endif

typedef struct {
    // backpointer because the radio is needed in init as it calls some generic
    // functions -- what happens when porting C++ code to C...
    lora_radio_t *radio;
    int8_t rst_pin;
    int8_t busy_pin;
    int8_t dio1_pin;
    int8_t rxen_pin;
    int8_t txen_pin;
    spi_dev_handle_t spi_dev;
    uint8_t tx_power;
    uint8_t tx_packet_len; // last packet transmitted
    uint8_t rx_packet_len; // last packet received
    // saved params, TODO refactor this......
    uint8_t regulator_mode;
    uint8_t packet_type;
    uint8_t frequency;
    uint8_t offset;
    uint8_t spreading_factor;
    uint8_t bandwidth;
    uint8_t code_rate;
    uint8_t optimization;
    uint16_t preamble;
    uint8_t header_type;
    uint8_t packet_len;
    uint8_t CRC_mode;
    uint8_t IQ_mode;
    uint16_t irq_mask;
    uint16_t dio1_mask;
    uint16_t dio2_mask;
    uint16_t dio3_mask;
} sx126x_t;

static const char *TAG = "SX126x";

static bool sx126x_check_device(lora_radio_t *r);
static void sx126x_reset_device(lora_radio_t *r);
static void sx126x_apply_config(lora_radio_t *r);
static bool sx126x_check_busy(lora_radio_t *r);
static void sx126x_set_mode(lora_radio_t *r, uint8_t mode);
static void sx126x_set_regulator_mode(lora_radio_t *r, uint8_t mode);
static void sx126x_set_packet_type(lora_radio_t *r, uint8_t packet_type);
static void sx126x_set_frequency(lora_radio_t *r, uint64_t freq,
                                 int32_t offset);
static void sx126x_set_PA_config(lora_radio_t *r, uint8_t duty_cycle,
                                 uint8_t hp_max);
static void sx126x_calibrate_image(lora_radio_t *r, uint32_t freq);
static void sx126x_set_modulation_params(lora_radio_t *r,
                                         uint8_t spreading_factor,
                                         uint8_t bandwidth, uint8_t code_rate,
                                         uint8_t optimization);
static void sx126x_set_packet_params(lora_radio_t *r, uint16_t preamble,
                                     uint8_t header_type, uint8_t packet_len,
                                     uint8_t CRC_mode, uint8_t IQ_mode);
static void sx126x_set_DIO_IRQ_params(lora_radio_t *r, uint16_t irq_mask,
                                      uint16_t dio1_mask, uint16_t dio2_mask,
                                      uint16_t dio3_mask);
static void sx126x_set_buf_base_addr(lora_radio_t *r, uint8_t tx_base_addr,
                                     uint8_t rx_base_addr);
static void sx126x_set_tx_params(lora_radio_t *r, int8_t tx_power,
                                 uint8_t ramp_time);
static void sx126x_set_sync_word(lora_radio_t *r, uint16_t sync_word);
static void sx126x_set_high_sensitivity(lora_radio_t *r);
static void sx126x_clear_IRQ_status(lora_radio_t *r, uint16_t irq_mask);
static void sx126x_set_tx(lora_radio_t *r, uint32_t timeout);
static void sx126x_set_rx(lora_radio_t *r, uint32_t timeout);
static int sx126x_read_IRQ_status(lora_radio_t *r);
static int sx126x_transmit(lora_radio_t *r, uint8_t *data, size_t len,
                           uint32_t timeout_ms);
static int sx126x_receive(lora_radio_t *r, uint8_t *rx_buf, size_t max_len,
                          uint32_t timeout_ms);
static bool sx126x_driver_init(lora_radio_t *r, const lora_config_t *config);

// utils at the bottom
static uint32_t get_actual_bandwidth(uint8_t bw_reg_value);
static float calc_symbol_ms(float bw, uint8_t sf);
static uint8_t calc_optimization(uint8_t bw, uint8_t sf);

static void sx126x_write_register_bytes(sx126x_t *c, uint16_t addr,
                                        uint8_t *buf, uint16_t len) {
    sx126x_check_busy(c->radio);

    uint8_t tx_buf[2 + len];
    tx_buf[0] = (addr >> 8) & 0xFF; // addr high
    tx_buf[1] = addr & 0xFF;        // addr low
    memcpy(&tx_buf[2], buf, len);

    if (!spi_hal_write_register_cmd(c->spi_dev, RADIO_WRITE_REGISTER, tx_buf,
                                    2 + len)) {
        LOG_ERROR(TAG, "FAILED TO WRITE REGISTER");
    }
}

static void write_register(sx126x_t *c, uint16_t address, uint8_t value) {
    return sx126x_write_register_bytes(c, address, &value, 1);
}

void write_command(sx126x_t *c, uint8_t command, uint8_t *buffer,
                   uint16_t size) {
    if (!sx126x_check_busy(c->radio)) {
        LOG_ERROR(TAG, "Busy timeout BEFORE command 0x%02X", command);
    }

    spi_hal_write_register_cmd(c->spi_dev, command, buffer, size);

    if (command != RADIO_SET_SLEEP) {
        sx126x_check_busy(c->radio);
    }
}

void read_command(sx126x_t *c, uint8_t command, uint8_t *buffer,
                  uint16_t size) {
    sx126x_check_busy(c->radio);

    // [COMMAND] [0xFF for status] [0xFF * size for data]
    uint8_t tx_buf[1 + size];
    memset(tx_buf, 0xFF, 1 + size); // all 0xFF

    uint8_t rx_buf[1 + size];

    spi_hal_read_register_cmd(c->spi_dev, command, rx_buf, tx_buf, 1 + size);

    memcpy(buffer, &rx_buf[1], size);
}

static void sx126x_read_register_bytes(sx126x_t *c, uint16_t addr, uint8_t *buf,
                                       uint16_t len) {
    uint8_t tx_buf[3 + len]; // addr_h + addr_l + status + data
    tx_buf[0] = addr >> 8;   // MSB
    tx_buf[1] = addr & 0xFF; // LSB
    tx_buf[2] = 0xFF;        // NOP
    memset(&tx_buf[3], 0xFF, len);

    uint8_t rx_buf[3 + len];

    if (!spi_hal_read_register_cmd(c->spi_dev, RADIO_READ_REGISTER, rx_buf,
                                   tx_buf, 3 + len)) {
        LOG_ERROR(TAG, "FAILED TO READ REGISTER");
        return;
    }

    memcpy(buf, &rx_buf[3], len);
}

static uint8_t read_register(sx126x_t *r, uint16_t address) {
    uint8_t data;
    sx126x_read_register_bytes(r, address, &data, 1);
    return data;
}

static bool rx_done(sx126x_t *c) { return gpio_get_level(c->dio1_pin); }
static bool tx_done(sx126x_t *c) { return gpio_get_level(c->dio1_pin); }

static void tx_enable(sx126x_t *c) {
    gpio_set_pin_level(c->rxen_pin, false);
    gpio_set_pin_level(c->txen_pin, true);
}

static void rx_enable(sx126x_t *c) {
    gpio_set_pin_level(c->txen_pin, false);
    gpio_set_pin_level(c->rxen_pin, true);
}

static sx126x_t *sx126x_chip_init_on_pins(lora_radio_t *r, int8_t cs_pin,
                                          int8_t rst_pin, int8_t busy_pin,
                                          int8_t dio1_pin, int8_t rxen_pin,
                                          int8_t txen_pin) {
    sx126x_t *c = malloc(sizeof(sx126x_t));
    if (!c) {
        return NULL;
    }

    r->chip = c;
    c->radio = r;

    c->rst_pin = rst_pin;
    c->busy_pin = busy_pin;
    c->dio1_pin = dio1_pin;
    c->rxen_pin = rxen_pin;
    c->txen_pin = txen_pin;

    spi_dev_cfg_t cfg = spi_hal_create_config(cs_pin, 8E6, 0);
    cfg.command_bits = 8;
    cfg.address_bits = 0;
    c->spi_dev = spi_hal_add_device(&cfg);
    if (!c->spi_dev) {
        r->chip = NULL;
        free(c);
        return NULL;
    }

    gpio_init_pin(c->rst_pin);
    gpio_set_pin_dir(c->rst_pin, GPIO_OUTPUT);
    gpio_set_pin_level(c->rst_pin, 0);

    gpio_init_pin(c->busy_pin);
    gpio_set_pin_dir(c->busy_pin, GPIO_INPUT);

    gpio_init_pin(c->dio1_pin);
    gpio_set_pin_dir(c->dio1_pin, GPIO_INPUT);

    gpio_init_pin(c->rxen_pin);
    gpio_set_pin_dir(c->rxen_pin, GPIO_OUTPUT);

    gpio_init_pin(c->txen_pin);
    gpio_set_pin_dir(c->txen_pin, GPIO_OUTPUT);

    sx126x_reset_device(r);

    if (!sx126x_check_device(r)) {
        LOG_ERROR(TAG, "failed to initialize lora chip");
        spi_hal_remove_device(c->spi_dev);
        r->chip = NULL;
        free(c);
        return NULL;
    } else {
        LOG_ERROR(TAG, "successfully checked lora device");
    }

    return c;
}

static sx126x_t *sx126x_chip_init(lora_radio_t *r) {
    return sx126x_chip_init_on_pins(r, CS_PIN, RST_PIN, BUSY_PIN, DIO1_PIN,
                                    RXEN_PIN, TXEN_PIN);
}

static void sx126x_apply_config(lora_radio_t *r) {
    sx126x_t *c = (sx126x_t *)r->chip;

    sx126x_reset_device(r);
    sx126x_set_mode(r, MODE_STDBY_RC);
    sx126x_set_regulator_mode(r, c->regulator_mode);
    sx126x_set_packet_type(r, c->packet_type);
    sx126x_set_frequency(r, c->frequency, c->offset);
    sx126x_set_modulation_params(r, c->spreading_factor, c->bandwidth,
                                 c->code_rate, LDRO_ON);
    sx126x_set_packet_params(r, c->preamble, c->header_type, c->packet_len,
                             c->CRC_mode, c->IQ_mode);
    sx126x_set_DIO_IRQ_params(r, c->irq_mask, c->dio1_mask, c->dio2_mask,
                              c->dio3_mask);
}

static void sx126x_set_mode(lora_radio_t *r, uint8_t mode) {
    sx126x_t *c = (sx126x_t *)r->chip;

    write_command(c, RADIO_SET_STANDBY, &mode, 1);

    // operating_mode = mode; // TODO
}

static void sx126x_set_regulator_mode(lora_radio_t *r, uint8_t mode) {
    sx126x_t *c = (sx126x_t *)r->chip;

    c->regulator_mode = mode;

    write_command(c, RADIO_SET_REGULATORMODE, &mode, 1);
}

static void sx126x_set_packet_type(lora_radio_t *r, uint8_t packet_type) {
    sx126x_t *c = (sx126x_t *)r->chip;

    c->packet_type = packet_type;

    write_command(c, RADIO_SET_PACKETTYPE, &packet_type, 1);
}

static void sx126x_set_frequency(lora_radio_t *r, uint64_t frequency,
                                 int32_t offset) {
    sx126x_t *c = (sx126x_t *)r->chip;

    c->frequency = frequency;
    c->offset = offset;

    uint8_t buf[4];
    uint32_t local_freq;

    local_freq = frequency + offset;

    local_freq = (uint32_t)((double)local_freq / (double)FREQ_STEP);

    buf[0] = (local_freq >> 24) & 0xFF; // MSB
    buf[1] = (local_freq >> 16) & 0xFF;
    buf[2] = (local_freq >> 8) & 0xFF;
    buf[3] = local_freq & 0xFF; // LSB

    write_command(c, RADIO_SET_RFFREQUENCY, buf, 4);
}

static void sx126x_set_modulation_params(lora_radio_t *r,
                                         uint8_t spreading_factor,
                                         uint8_t bandwidth, uint8_t code_rate,
                                         uint8_t optimization) {

    sx126x_t *c = (sx126x_t *)r->chip;

    c->spreading_factor = spreading_factor;
    c->bandwidth = bandwidth;
    c->code_rate = code_rate;

    if (optimization == LDRO_AUTO) {
        optimization = calc_optimization(bandwidth, spreading_factor);
    }

    c->optimization = optimization;

    uint8_t buf[4];

    buf[0] = spreading_factor;
    buf[1] = bandwidth;
    buf[2] = code_rate;
    buf[3] = optimization;

    write_command(c, RADIO_SET_MODULATIONPARAMS, buf, 4);
}

static void sx126x_set_packet_params(lora_radio_t *r, uint16_t preamble_len,
                                     uint8_t header_type, uint8_t packet_len,
                                     uint8_t CRC_mode, uint8_t IQ_mode) {
    sx126x_t *c = (sx126x_t *)r->chip;

    uint8_t preamble_MSB, preamble_LSB;

    preamble_MSB = preamble_len >> 8;
    preamble_LSB = preamble_len & 0xFF;

    c->preamble = preamble_len;
    c->header_type = header_type;
    c->packet_len = packet_len;
    c->CRC_mode = CRC_mode;
    c->IQ_mode = IQ_mode;

    uint8_t buf[9];
    buf[0] = preamble_MSB;
    buf[1] = preamble_LSB;
    buf[2] = header_type;
    buf[3] = packet_len;
    buf[4] = CRC_mode;
    buf[5] = IQ_mode;
    buf[6] = 0xFF;
    buf[7] = 0xFF;
    buf[8] = 0xFF;

    write_command(c, RADIO_SET_PACKETPARAMS, buf, 9);
}

static void sx126x_set_DIO_IRQ_params(lora_radio_t *r, uint16_t irq_mask,
                                      uint16_t dio1_mask, uint16_t dio2_mask,
                                      uint16_t dio3_mask) {
    sx126x_t *c = (sx126x_t *)r->chip;

    c->irq_mask = irq_mask;
    c->dio1_mask = dio1_mask;
    c->dio2_mask = dio2_mask;
    c->dio3_mask = dio3_mask;

    uint8_t buf[8];

    buf[0] = (uint8_t)(irq_mask >> 8);
    buf[1] = (uint8_t)(irq_mask & 0xFF);
    buf[2] = (uint8_t)(dio1_mask >> 8);
    buf[3] = (uint8_t)(dio1_mask & 0xFF);
    buf[4] = (uint8_t)(dio2_mask >> 8);
    buf[5] = (uint8_t)(dio2_mask & 0xFF);
    buf[6] = (uint8_t)(dio3_mask >> 8);
    buf[7] = (uint8_t)(dio3_mask & 0xFF);
    write_command(c, RADIO_CFG_DIOIRQ, buf, 8);
}

static void sx126x_set_PA_config(lora_radio_t *r, uint8_t duty_cycle,
                                 uint8_t hp_max) {
    sx126x_t *c = (sx126x_t *)r->chip;

    uint8_t buf[4];

    buf[0] = duty_cycle;
    buf[1] = hp_max; // hp_max: 0x00~0x07; 7 for 22dbm
    buf[2] = 0;      // device selection: 0 = SX1262; 1 = SX1261; 0 = SX1268;
    buf[3] = 0x01;   // reserved, always 0x01

    write_command(c, RADIO_SET_PACONFIG, buf, 4);
}

static void sx126x_set_buf_base_addr(lora_radio_t *r, uint8_t tx_base_addr,
                                     uint8_t rx_base_addr) {
    sx126x_t *c = (sx126x_t *)r->chip;

    uint8_t buf[2];

    buf[0] = tx_base_addr;
    buf[1] = rx_base_addr;
    write_command(c, RADIO_SET_BUFFERBASEADDRESS, buf, 2);
}

static void sx126x_calibrate_image(lora_radio_t *r, uint32_t freq) {
    sx126x_t *c = (sx126x_t *)r->chip;

    uint8_t cal_freq[2];

    if (freq > 900000000) {
        cal_freq[0] = 0xE1;
        cal_freq[1] = 0xE9;
    } else if (freq > 850000000) {
        cal_freq[0] = 0xD7;
        cal_freq[1] = 0xD8;
    } else if (freq > 770000000) {
        cal_freq[0] = 0xC1;
        cal_freq[1] = 0xC5;
    } else if (freq > 460000000) {
        cal_freq[0] = 0x75;
        cal_freq[1] = 0x81;
    } else if (freq > 425000000) {
        cal_freq[0] = 0x6B;
        cal_freq[1] = 0x6F;
    }

    write_command(c, RADIO_CALIBRATEIMAGE, cal_freq, 2);
}

static void sx126x_set_tx_params(lora_radio_t *r, int8_t tx_power,
                                 uint8_t ramp_time) {

    sx126x_t *c = (sx126x_t *)r->chip;
    uint8_t buf[2];

    c->tx_power = tx_power;

    buf[0] = tx_power;
    buf[1] = ramp_time;
    write_command(c, RADIO_SET_TXPARAMS, buf, 2);
}

static void sx126x_set_sync_word(lora_radio_t *r, uint16_t sync_word) {
    sx126x_t *c = (sx126x_t *)r->chip;

    write_register(c, REG_LR_SYNCWORD, (sync_word >> 8) & 0xFF);
    write_register(c, REG_LR_SYNCWORD + 1, sync_word & 0xFF);
}

static void sx126x_set_high_sensitivity(lora_radio_t *r) {
    sx126x_t *c = (sx126x_t *)r->chip;

    write_register(c, REG_RX_GAIN, BOOSTED_GAIN);
}

static void sx126x_clear_IRQ_status(lora_radio_t *r, uint16_t irq_mask) {
    sx126x_t *c = (sx126x_t *)r->chip;

    uint8_t buf[2];

    buf[0] = (uint8_t)(irq_mask >> 8);
    buf[1] = (uint8_t)(irq_mask & 0xFF);
    write_command(c, RADIO_CLR_IRQSTATUS, buf, 2);
};

static void sx126x_set_tx(lora_radio_t *r, uint32_t timeout) {
    sx126x_t *c = (sx126x_t *)r->chip;
    uint8_t buf[3];

    sx126x_clear_IRQ_status(r, IRQ_RADIO_ALL);

    tx_enable(c);

    timeout = timeout << 6; // convert ms to units of 15.625us

    buf[0] = (timeout >> 16) & 0xFF;
    buf[1] = (timeout >> 8) & 0xFF;
    buf[2] = timeout & 0xFF;

    // change required for data sheet addendum 15.1
    uint8_t reg_value = read_register(c, REG_TX_MODULATION);
    if (c->bandwidth == LORA_BW_500) {
        // if bandwidth is 500k set bit 2 to 0, see datasheet 15.1.1
        write_register(c, REG_TX_MODULATION, (reg_value & 0xFB));
    } else {
        // if bandwidth is < 500k set bit 2 to 0 see datasheet 15.1.1
        write_register(c, REG_TX_MODULATION, (reg_value | 0x04));
    }

    write_command(c, RADIO_SET_TX, buf, 3);
}

static void sx126x_set_rx(lora_radio_t *r, uint32_t timeout) {
    sx126x_t *c = (sx126x_t *)r->chip;
    uint8_t buf[3];

    sx126x_clear_IRQ_status(r, IRQ_RADIO_ALL);

    rx_enable(c);

    timeout = timeout << 6; // convert units of 15.625us to ms

    buf[0] = (timeout >> 16) & 0xFF;
    buf[1] = (timeout >> 8) & 0xFF;
    buf[2] = timeout & 0xFF;
    write_command(c, RADIO_SET_RX, buf, 3);
}

static int sx126x_read_IRQ_status(lora_radio_t *r) {
    sx126x_t *c = (sx126x_t *)r->chip;

    uint16_t temp;
    uint8_t buf[2];

    read_command(c, RADIO_GET_IRQSTATUS, buf, 2);
    temp = ((buf[0] << 8) + buf[1]);
    return temp;
}

static bool sx126x_check_device(lora_radio_t *r) {
    sx126x_t *c = (sx126x_t *)r->chip;

    uint8_t reg_data1, reg_data2;
    sx126x_check_busy(r);
    reg_data1 = read_register(c, 0x88e); // frequency setting low byte
    sx126x_check_busy(r);
    write_register(c, 0x88e, (reg_data1 + 1));

    sx126x_check_busy(r);
    reg_data2 = read_register(c, 0x88e); // read changed value back
    sx126x_check_busy(r);
    write_register(c, 0x88e, reg_data1); // restore to original value

    return (reg_data2 == (reg_data1 + 1));
}

static bool sx126x_check_busy(lora_radio_t *r) {
    uint8_t counter = 0;
    sx126x_t *c = (sx126x_t *)r->chip;

    while (gpio_get_pin_level(c->busy_pin)) {
        vTaskDelay(1);
        counter++;

        if (counter > 10) {
            sx126x_reset_device(r);
            sx126x_set_mode(r, MODE_STDBY_RC);
            sx126x_apply_config(r);
            return false;
        }
    }

    return true;
}

static void sx126x_reset_device(lora_radio_t *r) {
    sx126x_t *c = (sx126x_t *)r->chip;

    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_pin_level(c->rst_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(2));
    gpio_set_pin_level(c->rst_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(25));

    sx126x_check_busy(r);
}

static bool sx126x_driver_init(lora_radio_t *r, const lora_config_t *config) {
    sx126x_t *c = (sx126x_t *)r->chip;
    c->tx_power = config->tx_power;

    sx126x_set_mode(r, MODE_STDBY_RC);
    sx126x_set_regulator_mode(r, USE_DCDC);
    // from STM32 code: pa_duty_cycle=0x04, hp_max=0x07
    sx126x_set_PA_config(r, 0x04, 0x07);
    sx126x_calibrate_image(r, config->frequency);
    sx126x_set_packet_type(r, PACKET_TYPE_LORA);
    sx126x_set_frequency(r, config->frequency, config->offset);
    sx126x_set_modulation_params(r, LORA_SF7, LORA_BW_125, LORA_CR_4_5,
                                 LDRO_AUTO);
    sx126x_set_buf_base_addr(r, 0, 0);
    sx126x_set_packet_params(r, 8, LORA_PACKET_VARIABLE_LENGTH, 255,
                             LORA_CRC_ON, LORA_IQ_NORMAL);
    sx126x_set_DIO_IRQ_params(r, IRQ_RADIO_ALL,
                              (IRQ_TX_DONE + IRQ_RX_TX_TIMEOUT), 0, 0);
    sx126x_set_tx_params(r, config->tx_power, RADIO_RAMP_200_US);
    sx126x_set_high_sensitivity(r);
    sx126x_set_sync_word(r, LORA_MAC_PRIVATE_SYNCWORD);

    return true;
}

static int sx126x_receive(lora_radio_t *r, uint8_t *rx_buf, size_t max_len,
                          uint32_t timeout_ms) {
    // sx126x_t *c = (sx126x_t *)r->chip;

    return 0;
}

/* static bool write_buffer(sx126x_t *c, uint8_t offset, const uint8_t *data,
                         uint8_t len) {
    sx126x_check_busy(c->radio);

    // [OFFSET][DATA...]
    uint8_t tx_buf[1 + len];
    tx_buf[0] = offset;
    memcpy(&tx_buf[1], data, len);

    return spi_hal_write_register_cmd(c->spi_dev, RADIO_WRITE_BUFFER, tx_buf,
                                      1 + len);
} */
static bool write_buffer(sx126x_t *c, uint8_t offset, const uint8_t *data,
                         uint8_t len) {
    sx126x_check_busy(c->radio);

    uint8_t tx_buf[1 + len];
    tx_buf[0] = offset;
    memcpy(&tx_buf[1], data, len);

    return spi_hal_write_register_cmd(c->spi_dev, RADIO_WRITE_BUFFER, tx_buf,
                                      1 + len);
}

static int sx126x_transmit(lora_radio_t *r, uint8_t *data, size_t len,
                           uint32_t timeout_ms) {
    sx126x_t *c = (sx126x_t *)r->chip;

    if (len == 0) {
        return false;
    }

    sx126x_set_mode(r, MODE_STDBY_RC);
    sx126x_set_buf_base_addr(r, 0, 0);

    sx126x_check_busy(r);

    if (!write_buffer(c, 0, data, len)) {
        LOG_ERROR(TAG, "FAILED TO WRITE BUFFER");
        return 0;
    }

    c->tx_packet_len = len;
    write_register(c, REG_LR_PAYLOADLENGTH, c->tx_packet_len);

    sx126x_set_DIO_IRQ_params(r, IRQ_RADIO_ALL,
                              (IRQ_TX_DONE + IRQ_RX_TX_TIMEOUT), 0, 0);
    sx126x_set_tx(r, timeout_ms); // this starts the TX

    while (!tx_done(c)) {
        vTaskDelay(1);
    }

    // check for timeout
    if (sx126x_read_IRQ_status(r) & IRQ_RX_TX_TIMEOUT) {
        return 0;
    }

    return c->tx_packet_len;
}

lora_radio_t *lora_create_sx126x_radio(void) {
    lora_radio_t *radio = malloc(sizeof(lora_radio_t));
    if (!radio)
        return NULL;

    sx126x_t *c = sx126x_chip_init(radio);
    if (!c) {
        free(radio);
        return NULL;
    }

    radio->name = TAG;
    // radio->chip = c; // done internally in chip init
    radio->init = sx126x_driver_init;
    radio->calibrate_image = sx126x_calibrate_image;
    radio->clear_IRQ_status = sx126x_clear_IRQ_status;
    radio->read_IRQ_status = sx126x_read_IRQ_status;
    // TODO might need to restructure these to not be called in the chip init
    // radio->check_device = sx126x_check_device;
    // radio->reset_device = sx126x_reset_device;

    radio->set_mode = sx126x_set_mode;
    radio->set_packet_type = sx126x_set_packet_type;
    radio->set_frequency = sx126x_set_frequency;
    radio->set_tx_params = sx126x_set_tx_params;
    radio->set_modulation_params = sx126x_set_modulation_params;
    radio->set_buf_base_addr = sx126x_set_buf_base_addr;
    radio->set_packet_params = sx126x_set_packet_params;
    radio->set_sync_word = sx126x_set_sync_word;
    radio->set_high_sensitivity = sx126x_set_high_sensitivity;
    radio->set_DIO_IRQ_params = sx126x_set_DIO_IRQ_params;
    radio->set_tx = sx126x_set_tx;
    radio->set_rx = sx126x_set_rx;

    radio->transmit = sx126x_transmit;
    radio->receive = sx126x_receive;

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

    case 8:
        return 10400;

    case 1:
        return 15600;

    case 9:
        return 20800;

    case 2:
        return 31200;

    case 10:
        return 41700;

    case 3:
        return 62500;

    case 4:
        return 125000;

    case 5:
        return 250000;

    case 6:
        return 500000;

    default:
        return 0xFFFF;
    }
}
