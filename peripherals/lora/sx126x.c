#include "sx126x.h"
#include "gpio_hal.h"
#include "lora.h"
#include "spi_hal.h"

#include "sx126x_defs.h"

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
    int8_t cs_pin;
    int8_t rst_pin;
    int8_t busy_pin;
    int8_t dio1_pin;
    int8_t rxen_pin;
    int8_t txen_pin;
    spi_dev_handle_t spi_dev;
    bool rxtx_pinmode;
} sx126x_t;

static const char *TAG = "SX126x";

static void sx126x_set_mode(sx126x_t *r, uint8_t mode);
static bool sx126x_check_device(sx126x_t *r);
static void sx126x_apply_config(sx126x_t *r);
static void sx126x_check_busy(sx126x_t *r);
static void sx126x_reset_device(sx126x_t *r);

static void sx126x_write_register_bytes(sx126x_t *c, uint16_t addr,
                                        uint8_t *buf, uint16_t len) {
    // uint8_t tx_buf[2 + len];
    // tx_buf[0] = address >> 8;   // MSB
    // tx_buf[1] = address & 0xFF; // LSB
    // memcpy(&tx_buf[2], buf, len);

    if (!spi_hal_write_register_cmd_addr(c->spi_dev, RADIO_WRITE_REGISTER, addr,
                                         buf, len)) {
        ESP_LOGE(TAG, "FAILED TO TRANSMIT SPI VAL TO ADDR %d", addr);
    }
    // if (!spi_hal_write_register(c->spi_dev, RADIO_WRITE_REGISTER, tx_buf,
    //                             2 + len)) {
    //     ESP_LOGE(TAG, "FAILED TO TRANSMIT SPI VAL TO ADDR %d", address);
    // }
}

static void write_register(sx126x_t *c, uint16_t address, uint8_t value) {
    return sx126x_write_register_bytes(c, address, &value, 1);
}

static void sx126x_read_register_bytes(sx126x_t *c, uint16_t addr, uint8_t *buf,
                                       uint16_t len) {
    uint8_t tx_buf[1 + len];
    // tx_buf[0] = address >> 8;      // MSB
    // tx_buf[1] = address & 0xFF;    // LSB
    tx_buf[0] = 0xFF;              // NOP
    memset(&tx_buf[1], 0xFF, len); // rest of TX with NOPS

    uint8_t rx_buf[1 + len];

    if (!spi_hal_read_register_cmd_addr(c->spi_dev, RADIO_READ_REGISTER, addr,
                                        rx_buf, tx_buf, 1 + len)) {
        ESP_LOGE(TAG, "FAILED TO READ SPI VAL FROM ADDR %d", addr);
    }
    // if (!spi_hal_read_register(c->spi_dev, RADIO_READ_REGISTER,
    //                                tx_buf, 3 + len)) {
    //     ESP_LOGE(TAG, "FAILED TO READ SPI VAL FROM ADDR %d", address);
    // }

    memcpy(buf, &rx_buf[1], len);
}

static uint8_t read_register(sx126x_t *r, uint16_t address) {
    uint8_t data;
    sx126x_read_register_bytes(r, address, &data, 1);
    return data;
}

static sx126x_t *sx126x_chip_init_on_pins(int8_t cs_pin, int8_t rst_pin,
                                          int8_t busy_pin, int8_t dio1_pin,
                                          int8_t rxen_pin, int8_t txen_pin) {
    sx126x_t *r = malloc(sizeof(sx126x_t));
    if (!r) {
        return NULL;
    }

    r->cs_pin = cs_pin;
    r->rst_pin = rst_pin;
    r->busy_pin = busy_pin;
    r->dio1_pin = dio1_pin;
    r->rxen_pin = rxen_pin;
    r->txen_pin = txen_pin;

    spi_dev_cfg_t cfg = spi_hal_create_config(cs_pin, 8E6, 0);
    cfg.command_bits = 8;
    cfg.address_bits = 16;
    r->spi_dev = spi_hal_add_device(&cfg);
    if (!r->spi_dev) {
        free(r);
        return NULL;
    }

    gpio_init_pin(r->rst_pin);
    gpio_set_pin_dir(r->rst_pin, GPIO_OUTPUT);
    gpio_set_pin_level(r->rst_pin, 0);

    gpio_init_pin(r->busy_pin);
    gpio_set_pin_dir(r->rst_pin, GPIO_INPUT);

    gpio_init_pin(r->dio1_pin);
    gpio_set_pin_dir(r->dio1_pin, GPIO_INPUT);

    gpio_init_pin(r->rxen_pin);
    gpio_set_pin_dir(r->rxen_pin, GPIO_OUTPUT);

    gpio_init_pin(r->txen_pin);
    gpio_set_pin_dir(r->txen_pin, GPIO_OUTPUT);

    sx126x_reset_device(r);

    if (!sx126x_check_device(r)) {
        LOG_ERROR(TAG, "failed to initialize lora chip");
        spi_hal_remove_device(r->spi_dev);
        free(r);
        return NULL;
    }

    return r;
}

static sx126x_t *sx126x_chip_init() {
    return sx126x_chip_init_on_pins(CS_PIN, RST_PIN, BUSY_PIN, DIO1_PIN,
                                    RXEN_PIN, TXEN_PIN);
}

static void sx126x_set_mode(sx126x_t *r, uint8_t mode) {}

static void sx126x_apply_config(sx126x_t *r) {}

static bool sx126x_check_device(sx126x_t *r) {

    uint8_t reg_data1, reg_data2;
    sx126x_check_busy(r);
    reg_data1 = read_register(r, 0x88e); // frequency setting low byte
    sx126x_check_busy(r);
    write_register(r, 0x88e, (reg_data1 + 1));

    sx126x_check_busy(r);
    reg_data2 = read_register(r, 0x88e); // read changed value back
    sx126x_check_busy(r);
    write_register(r, 0x88e, reg_data1); // restore to original value

    LOG_ERROR(TAG, "reg_data1: %d | reg_data2: %d", reg_data1, reg_data2);

    return (reg_data2 == (reg_data1 + 1));
}

static void sx126x_check_busy(sx126x_t *r) {
    uint8_t counter = 0;

    while (gpio_get_pin_level(r->busy_pin)) {
        vTaskDelay(pdMS_TO_TICKS(1));
        counter++;

        if (counter > 10) {
            sx126x_reset_device(r);
            // sx126x_set_mode(r, 123);
            // sx126x_apply_config(r);
            break;
        }
    }
}

static void sx126x_reset_device(sx126x_t *r) {
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_pin_level(r->rst_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(2));
    gpio_set_pin_level(r->rst_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(25));

    sx126x_check_busy(r);
}

static bool sx126x_driver_init(lora_radio_t *radio,
                               const lora_config_t *config) {
    sx126x_t *c = radio->chip;

    return true;
}

lora_radio_t *lora_create_sx126x_radio(void) {
    lora_radio_t *radio = malloc(sizeof(lora_radio_t));
    if (!radio)
        return NULL;

    sx126x_t *r = sx126x_chip_init();
    if (!r) {
        free(radio);
        return NULL;
    }

    radio->name = TAG;
    radio->chip = r;
    radio->init = sx126x_driver_init;
    // radio->transmit = sx126x_driver_transmit;
    // radio->receive = sx126x_driver_receive;
    // radio->set_frequency = sx126x_driver_set_frequency;

    return radio;
}
