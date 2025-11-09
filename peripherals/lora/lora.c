#include "lora.h"
#include "gpio_hal.h"
#include "spi_hal.h"

#include "SX126X_defs.h"

#define CS_PIN (11)
#define RST_PIN (12)
#define BUSY_PIN (4)
#define DIO1_PIN (6)
#define RXEN_PIN (1)
#define TXEN_PIN (2)

static const char* TAG = "LoRA Driver";

static int8_t _cs_pin;
static int8_t _rst_pin;
static int8_t _busy_pin;
static int8_t _dio1_pin;
static int8_t _rxen_pin;
static int8_t _txen_pin;

static spi_dev_handle_t lora_spi_dev;

static bool rxtx_pinmode;

static void LoRa_write_register_bytes(uint16_t address, uint8_t *buf,
                                      uint16_t len) {
    uint8_t tx_buf[3 + len];
    tx_buf[0] = RADIO_WRITE_REGISTER;
    tx_buf[1] = address >> 8;   // MSB
    tx_buf[2] = address & 0xFF; // LSB
    memcpy(&tx_buf[3], buf, len);

    spi_hal_transmit(lora_spi_dev, tx_buf, 3 + len);
}

static void LoRa_write_register(uint16_t address, uint8_t value) {
    return LoRa_write_register_bytes(address, &value, 1);
}

static void LoRa_read_register_bytes(uint16_t address, uint8_t *buf,
                                     uint16_t len) {
    uint8_t rx_buf[4] = {
        RADIO_READ_REGISTER,
        address >> 8,     // MSB
        address & 0x00FF, // LSB
        0xFF              // NOP
    };

    spi_hal_write_then_read(lora_spi_dev, rx_buf, 4, buf, len, 0xFF);
}

static uint8_t LoRa_read_register(uint16_t address) {
    uint8_t data;
    LoRa_read_register_bytes(address, &data, 1);
    return data;
}

bool LoRa_init_on_pins(int8_t cs_pin, int8_t rst_pin, int8_t busy_pin,
                       int8_t dio1_pin, int8_t rxen_pin, int8_t txen_pin) {

    _cs_pin = cs_pin;
    gpio_init_pin(_cs_pin);

    spi_dev_cfg_t cfg = spi_hal_create_config(cs_pin, 8E6, 0);
    lora_spi_dev = spi_hal_add_device(&cfg);

    _rst_pin = rst_pin;
    _busy_pin = busy_pin;
    _dio1_pin = dio1_pin;
    _rxen_pin = rxen_pin;
    _txen_pin = txen_pin;

    gpio_init_pin(_rst_pin);
    gpio_set_pin_dir(_rst_pin, GPIO_OUTPUT);
    gpio_set_pin_level(_rst_pin, 0);

    gpio_init_pin(_busy_pin);
    gpio_set_pin_dir(_rst_pin, GPIO_INPUT);

    gpio_init_pin(_dio1_pin);
    gpio_set_pin_dir(_dio1_pin, GPIO_INPUT);

    gpio_init_pin(_rxen_pin);
    gpio_set_pin_dir(_rxen_pin, GPIO_OUTPUT);

    gpio_init_pin(_txen_pin);
    gpio_set_pin_dir(_txen_pin, GPIO_OUTPUT);

    LoRa_reset_device();

    return LoRa_check_device();
}

bool LoRa_init() {
    return LoRa_init_on_pins(CS_PIN, RST_PIN, BUSY_PIN, DIO1_PIN, RXEN_PIN,
                             TXEN_PIN);
}

void LoRa_set_mode(uint8_t mode) {}

void LoRa_apply_config(void) {}

bool LoRa_check_device(void) {

    uint8_t reg_data1, reg_data2;
    reg_data1 = LoRa_read_register(0x88e); // frequency setting low byte
    LoRa_write_register(0x88e, (reg_data1 + 1));

    reg_data2 = LoRa_read_register(0x88e); // read changed value back
    LoRa_write_register(0x88e, reg_data1); // restore to original value

    LOG_ERROR(TAG, "reg_data1: %d | reg_data2: %d", reg_data1, reg_data2);

    return (reg_data2 == (reg_data1 + 1));
}

void LoRa_check_busy(void) {
    uint8_t counter = 0;

    while (gpio_get_pin_level(BUSY_PIN)) {
        vTaskDelay(pdMS_TO_TICKS(11));
        counter++;

        if (counter > 10) {
            LoRa_reset_device();
            LoRa_set_mode(1234);
            LoRa_apply_config();
            break;
        }
    }
}

void LoRa_reset_device(void) {
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_pin_level(RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(2));
    gpio_set_pin_level(RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(25));

    LoRa_check_busy();
}
