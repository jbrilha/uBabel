#pragma once

#include <stdbool.h>
#include <stdint.h>

#define IOT_CONTROL_PROTO_ID 17000

#define  DEVICE_TYPE_LED_RGB 1
#define  DEVICE_TYPE_LED_MATRIX 2
#define  DEVICE_TYPE_LCD_DISPLAY 3

typedef void* iot_node_handle_t;
typedef signed char iot_device_handle_t;

#define INVALID_NODE -2
#define NO_DEVICE -1

#ifdef __cplusplus
extern "C" {
#endif  

void iot_control_protocol_init();

iot_node_handle_t initialize_node_iterator();
iot_node_handle_t next_node(iot_node_handle_t node);
iot_node_handle_t previous_node(iot_node_handle_t node);
bool print_node_identifier(iot_node_handle_t node, char* str);

iot_device_handle_t initialize_device_iterator(iot_node_handle_t node);
iot_device_handle_t next_device(iot_node_handle_t node, iot_device_handle_t device);
iot_device_handle_t previous_device(iot_node_handle_t node, iot_device_handle_t device);

uint8_t get_device_type(iot_node_handle_t node, iot_device_handle_t device);

bool activate_led(iot_node_handle_t node);

#ifdef __cplusplus
}
#endif  
