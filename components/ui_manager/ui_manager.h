#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "platform.h"

void ui_manager_init(void);

void ui_manager_set_lora_rec_widget();

void ui_manager_set_lora_sndr_widget();

void ui_manager_set_temperature_widget();

void ui_manager_set_messenger_widget();

void ui_manager_set_tardis_widget();
void ui_manager_update_tardis_widget();

#endif // !UI_MANAGER_H

