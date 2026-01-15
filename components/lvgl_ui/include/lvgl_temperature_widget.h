#ifndef LVGL_TEMPERATURE_WIDGET_H
#define LVGL_TEMPERATURE_WIDGET_H

#include "lvgl.h"

#define UI_EVENT_REQ_TEMP 1001
#define UI_EVENT_REC_TEMP 1002

void temperature_widget_init(lv_display_t *disp, bool use_style,
                             bool animate);
void temperature_widget_init_on_container(lv_obj_t *container,
                                          bool use_style, bool animate);

void temperature_bar_set_val(int32_t temp);
void temperature_bar_animate_to_val(int32_t temp);
lv_obj_t *get_ui_bar();

#endif
