#ifndef LVGL_CAMERA_WIDGET_H
#define LVGL_CAMERA_WIDGET_H

#include "lvgl.h"

void camera_widget_init(lv_display_t *disp, _lock_t *lock);
void camera_widget_init_on_container(lv_obj_t *container, _lock_t *lock);

void camera_widget_set_feed(uint8_t *buf, size_t len);

#endif
