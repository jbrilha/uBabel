#ifndef ESP_LORA_H
#define ESP_LORA_H

#include "lora_types.h"
#include "lora_events.h"

bool esp_lora_init(void);

bool esp_lora_start_sender(void);
bool esp_lora_start_receiver(void);

void esp_lora_transmit_packet(void);

#endif // !ESP_LORA_H
