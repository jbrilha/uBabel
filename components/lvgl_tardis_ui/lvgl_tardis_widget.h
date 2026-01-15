#ifndef LVGL_TARDIS_WIDGET_H
#define LVGL_TARDIS_WIDGET_H

#include "proto_iot_control.h"
#include "event.h"
#include "network_events.h"
#include "lvgl.h"

void tardis_widget_init(lv_display_t *disp);
void tardis_widget_init_on_container(lv_obj_t *container);

void tardis_widget_set_notif_txt(const char *notif);

void tardis_widget_set_network_up_info(network_event_t *e);
void tardis_widget_set_network_down(void);

void tardis_widget_populate_menu(void);

void tardis_widget_menu_next(void);

void tardis_widget_menu_prev(void);

void tardis_widget_menu_select(void);

void tardis_widget_menu_back(void);

#endif
