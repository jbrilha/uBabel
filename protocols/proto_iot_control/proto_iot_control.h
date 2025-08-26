#pragma once

#define IOT_CONTROL_PROTO_ID 17000

#define DEVICE_TYPE_LED_RGB 1
#define  DEVICE_TYPE_LED_MATRIX 2
#define  DEVICE_TYPE_LCD_DISPLAY 3

#ifdef __cplusplus
extern "C" {
#endif  

void iot_control_protocol_init();

#ifdef __cplusplus
}
#endif  
