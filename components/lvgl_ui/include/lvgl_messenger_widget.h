#ifndef LVGL_MESSENGER_WIDGET_H
#define LVGL_MESSENGER_WIDGET_H

#include "lvgl.h"

#define UI_EVENT_SND_MSG 1101
#define UI_EVENT_REC_MSG 1102

void messenger_widget_init(lv_display_t *disp);
void messenger_widget_init_on_container(lv_obj_t *container);

void messenger_widget_set_txt(const char *txt);

#endif
