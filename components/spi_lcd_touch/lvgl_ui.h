#ifndef LVGL_UI_H
#define LVGL_UI_H

#include "lvgl.h"

void lvgl_demo_ui_init(lv_display_t *disp);

lv_obj_t *temperature_bar_create(lv_obj_t *container, bool use_style,
                                 bool animate);
void temperature_bar_init(lv_display_t *disp, _lock_t *lock, bool use_style,
                          bool animate);
void temperature_bar_init_on_container(lv_obj_t *container, _lock_t *lock,
                                       bool use_style, bool animate);
void temperature_bar_set_val(int32_t temp);
void temperature_bar_animate_to_val(int32_t temp);
lv_obj_t *get_ui_bar();

void lvgl_flex_layout_init(lv_display_t *disp, _lock_t *lock);
lv_obj_t *lvgl_flex_layout_get_col(int32_t col_idx);

#endif
