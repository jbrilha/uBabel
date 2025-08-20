#ifndef LVGL_UI_H
#define LVGL_UI_H

#include "lvgl.h"

void lvgl_demo_ui_init(lv_display_t *disp);

void lvgl_temp_bar_init(lv_display_t *disp, _lock_t *lock);
void lvgl_temp_bar_set(int32_t temp);
void lvgl_temp_bar_animate_to(int32_t temp);

#endif
