#ifndef SPI_LCD_H
#define SPI_LCD_H

#include "lvgl.h"
#include "freertos/FreeRTOS.h"

void lcd_init_task(void *pvParameters);

SemaphoreHandle_t spi_lcd_get_lvgl_lock(void);
lv_display_t *spi_lcd_get_display(void);

bool is_display_initialized(void);

#endif // !SPI_LCD_H
