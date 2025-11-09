#ifndef LORA_H
#define LORA_H

#include "platform.h"
#include <stdbool.h>

bool LoRa_init_on_pins(int8_t cs_pin, int8_t rst_pin, int8_t busy_pin,
                       int8_t dio1_pin, int8_t rxen_pin, int8_t txen_pin);
bool LoRa_init(void);

bool LoRa_check_device(void);

void LoRa_reset_device(void);
void LoRa_check_busy(void);
void LoRa_apply_config(void);
void LoRa_set_mode(uint8_t mode);

#endif // !LORA_H
