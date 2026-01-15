#ifndef LVGL_FLEX_LAYOUT_H
#define LVGL_FLEX_LAYOUT_H

#include "lvgl.h"
#include "freertos/FreeRTOS.h"

void lvgl_flex_layout_init(lv_display_t *disp, SemaphoreHandle_t lock);
lv_obj_t *lvgl_flex_layout_get_col(int32_t col_idx);

#endif
