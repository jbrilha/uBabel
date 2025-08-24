#ifndef SPI_LCD_H
#define SPI_LCD_H

#include "platform.h"

#define EVENT_ROTATION_0 901
#define EVENT_ROTATION_90 902
#define EVENT_ROTATION_180 903
#define EVENT_ROTATION_270 904 

void lcd_init_task(void *pvParameters);

#endif // !SPI_LCD_H

