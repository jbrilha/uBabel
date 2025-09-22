#include "m5_buttons.h"
#include "stdio.h"

#include "gpio_hal.h"
#include "platform.h"

#include "event_dispatcher.h"

#define M5_BUTTON_TASK_NAME "m5_button_task"
#define M5_BUTTON_TASK_STACK_SIZE 4096
#define M5_BUTTON_TASK_PRIORITY 1

#define BUTTON_COUNT 3

static const char *TAG = "BUTTON";

static int button_pin;
static int last_state = 0;
static int last_states[BUTTON_COUNT] = {0, 0, 0};

void button_init(int pin) {
    button_pin = pin;
    gpio_init_pin(pin);
    gpio_set_pin_dir(pin, GPIO_INPUT);
}

void buttons_task(void *params) {
    button_init(PIN_A);
    button_init(PIN_B);
    button_init(PIN_C);
    int state;

    while (1) {
        for (int i = 0; i < BUTTON_COUNT; i++) {
            state = gpio_get_pin_level(i + 37);
            if (state && state != last_states[i]) {

                event_t *event =
                    create_event(EVENT_TYPE_NOTIFICATION, 237 + i, NULL, 0);
                if (event) {
                    event_dispatcher_post(event);
                }
            }
            last_states[i] = state;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelete(NULL);
}

void run_m5_buttons_task(void) {
    xTaskCreate(buttons_task, M5_BUTTON_TASK_NAME, M5_BUTTON_TASK_STACK_SIZE,
                NULL, M5_BUTTON_TASK_PRIORITY, NULL);
}
