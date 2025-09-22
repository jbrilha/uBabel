#include "ui_manager.h"

#include "lvgl_tardis_widget.h"
#include "lvgl_ui.h"
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

    while (!is_display_initialized()) {
        vTaskDelay(pdMS_TO_TICKS(500));
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

void ui_manager_set_temperature_widget() {
    lv_display_t *display;
    _lock_t *lvgl_lock;

    if ((display = spi_lcd_get_display()) == NULL ||
        (lvgl_lock = spi_lcd_get_lvgl_lock()) == NULL) {
        return;
    }

    temperature_widget_init(display, lvgl_lock, true, true);
}

void ui_manager_set_messenger_widget() {
    lv_display_t *display;
    _lock_t *lvgl_lock;

    if ((display = spi_lcd_get_display()) == NULL ||
        (lvgl_lock = spi_lcd_get_lvgl_lock()) == NULL) {
        return;
    }

    lvgl_flex_layout_init(display, lvgl_lock);
    messenger_widget_init_on_container(lvgl_flex_layout_get_col(0), lvgl_lock);
    temperature_widget_init_on_container(lvgl_flex_layout_get_col(1), lvgl_lock,
                                         true, true);
}

static bool tardis_initialized = false;

void ui_manager_set_tardis_widget() {
    lv_display_t *display;
    _lock_t *lvgl_lock;

    if ((display = spi_lcd_get_display()) == NULL ||
        (lvgl_lock = spi_lcd_get_lvgl_lock()) == NULL) {
        return;
    }

    tardis_widget_init(display, lvgl_lock);

    tardis_initialized = true;
}

void ui_manager_update_tardis_widget() {
    if (!tardis_initialized)
        return;

    tardis_widget_populate_menu();
}
