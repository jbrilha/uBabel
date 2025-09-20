#ifndef LVGL_TARDIS_WIDGET_H
#define LVGL_TARDIS_WIDGET_H

#include "event.h"
#include "lvgl.h"

void tardis_widget_init(lv_display_t *disp, _lock_t *lock);
void tardis_widget_init_on_container(lv_obj_t *container, _lock_t *lock);

void tardis_widget_set_notif_txt(const char *notif);

void tardis_widget_set_info_from_event(event_t *e);

#endif
