#include "ui_manager.h"

#include "lvgl_lora_receiver_widget.h"
#include "lvgl_lora_sender_widget.h"
#include "misc/lv_types.h"
#include "spi_lcd_touch.h"
#include "ui_event_monitor.h"

static const char *TAG = "UI_MANAGER";

void ui_manager_init(void) {

    xTaskCreate(lcd_init_task,   // Task function
                "lcd_init_task", // Task name
                4096,            // Stack size in words
                NULL,            // Task parameters
                1,               // Priority
                NULL             // Task handle
    );

    // ensure display init before returning
    while (spi_lcd_get_display() == NULL || spi_lcd_get_lvgl_lock() == NULL) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ui_event_monitor_init();
}

void ui_manager_set_lora_rec_widget() {
    lv_display_t *display;
    _lock_t *lvgl_lock;

    if ((display = spi_lcd_get_display()) == NULL ||
        (lvgl_lock = spi_lcd_get_lvgl_lock()) == NULL) {
        return;
    }

    lora_rec_widget_init(display, lvgl_lock);
}

void ui_manager_set_lora_sndr_widget() {
    lv_display_t *display;
    _lock_t *lvgl_lock;

    if ((display = spi_lcd_get_display()) == NULL ||
        (lvgl_lock = spi_lcd_get_lvgl_lock()) == NULL) {
        return;
    }

    lora_sndr_widget_init(display, lvgl_lock);
}
