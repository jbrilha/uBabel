#ifndef LVGL_MESSENGER_WIDGET_H
#define LVGL_MESSENGER_WIDGET_H

#include "lvgl.h"

void messenger_widget_init(lv_display_t *disp, _lock_t *lock);
void messenger_widget_init_on_container(lv_obj_t *container, _lock_t *lock);

void messenger_widget_set_txt(const char *txt);

#endif
