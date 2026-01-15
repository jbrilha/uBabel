#ifndef LVGL_LORA_SENDER_WIDGET_H
#define LVGL_LORA_SENDER_WIDGET_H

#include "lora_events.h"
#include "lvgl.h"

void lora_sndr_widget_init(lv_display_t *disp);
void lora_sndr_widget_init_on_container(lv_obj_t *container);

void lora_sndr_widget_set_sender_txt(const char *sender);
void lora_sndr_widget_set_message_txt(const char *msg);
void lora_sndr_widget_set_message_id_txt(const char *msg);

void lora_sndr_widget_send_transmission(event_t *e);

#endif
