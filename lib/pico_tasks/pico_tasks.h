#ifndef PIMORONI_TASKS_H
#define PIMORONI_TASKS_H

#define EVENT_BUTTON_A_PRESSED 201
#define EVENT_BUTTON_B_PRESSED 202
#define EVENT_BUTTON_X_PRESSED 203
#define EVENT_BUTTON_Y_PRESSED 204 

void pico_buttons_init(void);
void pico_buttons_task(void *params);
void start_buttons_task(void);

void unicorn_task_init(void);
void unicorn_task(void *params);
void start_unicorn_task(void);

void scroll_task_init(void);
void scroll_task(void *params);
void start_scroll_task(void);

#endif
