#ifndef SPI_LCD_H
#define SPI_LCD_H

#include "lvgl.h"

void lcd_init_task(void *pvParameters);

lv_display_t *spi_lcd_get_display(void);

bool is_display_initialized(void);

#endif // !SPI_LCD_H
