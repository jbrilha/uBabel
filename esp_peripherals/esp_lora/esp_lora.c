#include <sx127x.h>

#include "platform.h"

#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <driver/spi_master.h>

#include "spi_hal.h"

#include "esp_lora.h"
#include "esp_lora_receiver.h"
#include "esp_lora_sender.h"

#define ISR_TASK_STACK_SIZE (4 * 1024)
#define ISR_TASK_PRIORITY 2

// LoRa params
// #define LORA_FREQ     433E6
#define LORA_FREQ 868E6
#define LORA_SF 12
#define LORA_BW 125E3
#define LORA_TX_POWER 17

#ifdef M5STACK_CORE_BASIC
#define CS_PIN 5
#define IRQ_PIN 36
#elif defined(CONFIG_IDF_TARGET_ESP32)
#define CS_PIN 5
#define IRQ_PIN 36
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define CS_PIN 17
#define IRQ_PIN 18
#endif

static const char *TAG = "sx127x";

sx127x device;
TaskHandle_t handle_interrupt;
int total_packets_received = 0;
static bool lora_initialized = false;

void IRAM_ATTR handle_interrupt_fromisr(void *arg) {
    xTaskResumeFromISR(handle_interrupt);
}

void handle_interrupt_task(void *arg) {
    while (1) {
        vTaskSuspend(NULL);
        sx127x_handle_interrupt((sx127x *)arg);
    }
}

void setup_gpio_interrupts(gpio_num_t gpio, sx127x *device) {
    gpio_set_direction(gpio, GPIO_MODE_INPUT);
#ifndef M5STACK_CORE_BASIC
    gpio_pulldown_en(gpio);
    gpio_pullup_dis(gpio);
#endif
    gpio_set_intr_type(gpio, GPIO_INTR_POSEDGE);
    gpio_isr_handler_add(gpio, handle_interrupt_fromisr, (void *)device);
}

spi_device_handle_t setup_spi_device() {
    spi_device_interface_config_t dev_cfg = {.clock_speed_hz = 4E6,
                                             .spics_io_num = CS_PIN,
                                             .queue_size = 16,
                                             .command_bits = 0,
                                             .address_bits = 8,
                                             .dummy_bits = 0,
                                             .mode = 0};
    return spi_hal_add_device(&dev_cfg);
}

void setup_sx127x() {
    spi_device_handle_t spi_device = setup_spi_device();
    ESP_ERROR_CHECK(sx127x_create(spi_device, &device));
    ESP_ERROR_CHECK(
        sx127x_set_opmod(SX127X_MODE_SLEEP, SX127X_MODULATION_LORA, &device));
    ESP_ERROR_CHECK(sx127x_set_frequency(LORA_FREQ, &device));
    ESP_ERROR_CHECK(sx127x_lora_reset_fifo(&device));
    ESP_ERROR_CHECK(sx127x_rx_set_lna_boost_hf(true, &device));
    ESP_ERROR_CHECK(
        sx127x_set_opmod(SX127X_MODE_STANDBY, SX127X_MODULATION_LORA, &device));
    ESP_ERROR_CHECK(sx127x_rx_set_lna_gain(SX127X_LNA_GAIN_G4, &device));
    ESP_ERROR_CHECK(sx127x_lora_set_bandwidth(SX127X_BW_125000, &device));
    ESP_ERROR_CHECK(sx127x_lora_set_implicit_header(NULL, &device));
    ESP_ERROR_CHECK(sx127x_lora_set_spreading_factor(SX127X_SF_9, &device));
    ESP_ERROR_CHECK(sx127x_lora_set_syncword(18, &device));
    ESP_ERROR_CHECK(sx127x_set_preamble_length(8, &device));
}

bool esp_lora_init() {
    setup_sx127x();

    return true;
}

bool start_isr_task() {
    ESP_LOGI(TAG, "starting ISR task");
    BaseType_t task_code = xTaskCreatePinnedToCore(
        handle_interrupt_task, "handle interrupt", 8196, &device, 2,
        &handle_interrupt, xPortGetCoreID());
    if (task_code != pdPASS) {
        ESP_LOGE(TAG, "can't create task %d", task_code);
        return false;
    }

    gpio_install_isr_service(0);
    setup_gpio_interrupts((gpio_num_t)IRQ_PIN, &device);

    return true;
}

bool esp_lora_start_receiver() {
    ESP_LOGI(TAG, "initializing receiver");
    if (!lora_initialized) {
        esp_lora_init();
        lora_initialized = true;
    }
    ESP_LOGI(TAG, "receiver initialized");

    configure_receiver(&device);
    ESP_LOGI(TAG, "receiver configured");

    return start_isr_task();
}

static void esp_lora_sender_task(void *pvParameters) {

    while (true) {
        esp_lora_transmit_packet();

        vTaskDelay(pdMS_TO_TICKS(3000));
    }

    vTaskDelete(NULL);
}

bool esp_lora_start_sender() {
    ESP_LOGI(TAG, "initializing sender");
    if (!lora_initialized) {
        esp_lora_init();
        lora_initialized = true;
    }
    ESP_LOGI(TAG, "sender initialized");

    configure_sender(&device);
    ESP_LOGI(TAG, "sender configured");

    if (!start_isr_task()) {
        ESP_LOGE(TAG, "failed to start isr task");
        return false;
    }
    ESP_LOGI(TAG, "ISR task started");

    return xTaskCreate(esp_lora_sender_task, "ESP_LORA_SENDER_TASK", 4096, NULL,
                       2, NULL) == pdPASS;
}

void esp_lora_transmit_packet() { transmit_packet_from_device(&device); }
