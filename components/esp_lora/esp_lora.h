#ifndef ESP_LORA_H
#define ESP_LORA_H

#include "platform.h"

void esp_lora_receiver_task(void *pvParameters);

void esp_lora_sender_task(void *pvParameters);

#ifdef __cplusplus
extern "C" {
#endif

void start_lora_sender_task();
void start_lora_receiver_task();

#ifdef __cplusplus
}
#endif

#endif // !ESP_LORA_H
