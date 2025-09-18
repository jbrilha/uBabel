#ifndef ESP_LORA_SENDER_H
#define ESP_LORA_SENDER_H

#include "platform.h"
#include "sx127x.h"

void transmit_packet_from_device(sx127x *device);

void tx_callback(sx127x *device);
void configure_sender(sx127x *device);

#endif // !ESP_LORA_SENDER_H
