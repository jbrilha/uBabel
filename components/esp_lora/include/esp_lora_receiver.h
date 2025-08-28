#ifndef ESP_LORA_RECEIVER_H
#define ESP_LORA_RECEIVER_H

#include "platform.h"
#include "sx127x.h"

void rx_callback(sx127x *device, uint8_t *data, uint16_t data_length);

void cad_callback(sx127x *device, int cad_detected);

void configure_receiver(sx127x *device);

#endif // !ESP_LORA_RECEIVER_H
