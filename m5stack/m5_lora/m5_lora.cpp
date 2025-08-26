#include <RadioLib.h>

// include the hardware abstraction layer
#include "esp_hal.h"

#include "m5_lora.h"

#define LORA_SENDER_TASK_STACK_SIZE (4 * 1024)
#define LORA_SENDER_TASK_PRIORITY 2

#define LORA_RECEIVER_TASK_STACK_SIZE (4 * 1024)
#define LORA_RECEIVER_TASK_PRIORITY 2

// create a new instance of the HAL class
#define CS_PIN 5
#define RST_PIN 13
#define IRQ_PIN 36

#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_SCLK 18

// LoRa Parameters Config
// #define LORA_FREQ     433E6
#define LORA_FREQ 868
#define LORA_SF 12
#define LORA_BW 125E3
#define LORA_TX_POWER 17
EspHal *hal = new EspHal(LORA_SCLK, LORA_MISO, LORA_MOSI);

SX1276 radio = new Module(hal, CS_PIN, IRQ_PIN, RST_PIN);

static const char *TAG = "ESP_LORA";

static bool lora_initialized = false;

void init_lora() {
    ESP_LOGI(TAG, "[SX1276] Initializing ... ");
    int state = radio.begin(LORA_FREQ);
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGI(TAG, "failed, code %d\n", state);
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    ESP_LOGI(TAG, "success!\n");

    radio.setOutputPower(LORA_TX_POWER);
    radio.setBandwidth(LORA_BW);
    radio.setSpreadingFactor(LORA_SF);

    radio.setCRC(true);

    lora_initialized = true;
}

void sender_task(void *pvParameters) {
    int state;

    int i = 0;
    char str[64];
    for (;;) {
        snprintf(str, sizeof(str), "Hello world! %d", i++);
        ESP_LOGI(TAG, "[SX1276] Transmitting packet %d... ", i);

        state = radio.transmit(str);
        if (state == RADIOLIB_ERR_NONE) {
            ESP_LOGI(TAG, "successfully transmitted LoRa packet");

        } else {
            ESP_LOGI(TAG, "failed to transmit LoRa packet, code %d\n", state);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete(NULL);
}

void receiver_task(void *pvParameters) {
    int state;
    uint8_t received_data[256];

    for (;;) {
        ESP_LOGI(TAG, "[SX1276] Waiting for incoming transmission ...");
        
        state = radio.receive(received_data, sizeof(received_data) - 1);
        
        if (state == RADIOLIB_ERR_NONE) {
            ESP_LOGI(TAG, "success!");
            
            size_t received_length = radio.getPacketLength();
            if (received_length < sizeof(received_data)) {
                received_data[received_length] = '\0';
            } else {
                received_data[sizeof(received_data) - 1] = '\0';
            }
            
            ESP_LOGI(TAG, "[SX1276] Data: %s", received_data);
            ESP_LOGI(TAG, "[SX1276] RSSI: %.2f dBm", radio.getRSSI());
            ESP_LOGI(TAG, "[SX1276] SNR: %.2f dB", radio.getSNR());
            
        } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
            ESP_LOGI(TAG, "timeout!");
        } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
            ESP_LOGI(TAG, "CRC error!");
        } else {
            ESP_LOGI(TAG, "failed, code %d", state);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelete(NULL);
}

extern "C" {
void start_lora_sender_task() {
    if (!lora_initialized) {
        init_lora();
    }

    xTaskCreate(sender_task, "LORA_SENDER", LORA_SENDER_TASK_STACK_SIZE, NULL,
                LORA_SENDER_TASK_PRIORITY, NULL);
}

void start_lora_receiver_task() {
    if (!lora_initialized) {
        init_lora();
    }

    xTaskCreate(receiver_task, "LORA_RECEIVER", LORA_RECEIVER_TASK_STACK_SIZE,
                NULL, LORA_RECEIVER_TASK_PRIORITY, NULL);
}
}
