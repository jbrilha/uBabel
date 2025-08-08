#ifndef PICO_TOUCH_SCREEN_H
#define PICO_TOUCH_SCREEN_H

#include "platform.h"

#define EVENT_TOUCH_DRAWING 1001
#define EVENT_TEXTBOX_DRAWING 1002
#define EVENT_TEST_RANDOM_LINES 1003
#define EVENT_TEST_RANDOM_LABELS 1004 

void pico_touch_screen_task(void *pvParameters);

#endif // !PICO_TOUCH_SCREEN_H

