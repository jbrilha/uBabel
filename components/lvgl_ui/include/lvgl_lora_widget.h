#ifndef LVGL_LORA_WIDGET_H
#define LVGL_LORA_WIDGET_H

#include "lvgl.h"
#include "lora_events.h"

void lora_widget_init(lv_display_t *disp, _lock_t *lock);
void lora_widget_init_on_container(lv_obj_t *container, _lock_t *lock);

void lora_widget_set_rssi(int32_t rssi);
void lora_widget_animate_to_rssi(int32_t rssi);

void lora_widget_set_snr(float snr);
void lora_widget_animate_to_snr(float snr);

void lora_widget_set_freq_err(int32_t err);

void lora_widget_set_sender_txt(const char *sender);
void lora_widget_set_message_txt(const char *msg);
void lora_widget_set_message_id_txt(const char *msg);

void lora_widget_set_info_from_event(event_t *e);

lv_obj_t *lora_get_widget();

#endif
