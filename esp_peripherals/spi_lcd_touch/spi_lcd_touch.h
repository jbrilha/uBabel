#ifndef SPI_LCD_H
#define SPI_LCD_H

#include "lvgl.h"

void lcd_init_task(void *pvParameters);

_lock_t *spi_lcd_get_lvgl_lock(void);
lv_display_t *spi_lcd_get_display(void);

#endif // !SPI_LCD_H
