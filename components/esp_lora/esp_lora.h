#ifndef ESP_LORA_H
#define ESP_LORA_H

#include "platform.h"
#include "lora_types.h"
#include "lora_events.h"

void init_lora_sx127x();
void start_lora_sender();
void start_lora_receiver();

#endif // !ESP_LORA_H
