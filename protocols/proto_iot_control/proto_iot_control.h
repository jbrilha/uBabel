#pragma once

#include <stdbool.h>

#define IOT_CONTROL_PROTO_ID 17000

#define  DEVICE_TYPE_LED_RGB 1
#define  DEVICE_TYPE_LED_MATRIX 2
#define  DEVICE_TYPE_LCD_DISPLAY 3

typedef void* iot_node_handler_t;

#ifdef __cplusplus
extern "C" {
#endif  

void iot_control_protocol_init();

iot_node_handler_t initialize_device_iterator();
iot_node_handler_t next_device(iot_node_handler_t device);
iot_node_handler_t previous_device(iot_node_handler_t device);
bool print_device_identifier(iot_node_handler_t device, char* str);
bool device_has_led(iot_node_handler_t device);
bool activate_led(iot_node_handler_t device);

#ifdef __cplusplus
}
#endif  
