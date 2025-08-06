#pragma once

#define EVENT_BUTTON_A_PRESSED 201
#define EVENT_BUTTON_B_PRESSED 202
#define EVENT_BUTTON_X_PRESSED 203
#define EVENT_BUTTON_Y_PRESSED 204 

#ifdef __cplusplus
extern "C" {
#endif  

void pico_buttons_init();

void pico_buttons_task(void *params);

#ifdef __cplusplus
}
#endif  
