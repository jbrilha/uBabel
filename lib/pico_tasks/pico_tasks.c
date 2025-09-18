#include "pico_tasks.h"
#include "pico_scroll.h"
#include "pico_unicorn.h"

#include <math.h>
#include <pico/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "common_events.h"
#include "event_dispatcher.h"
#include "network_events.h"
#include "platform.h"

#define SCROLL_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)
#define UNICORN_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)

#define SCROLL_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define UNICORN_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

// TODO these are also defined in the application files...
#define REQUEST_PRINT 800
#define REQUEST_SHOW 801

#define PICO_BUTTONS_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE)
#define PICO_BUTTONS_TASK_PRIORITY (tskIDLE_PRIORITY + 4UL)

#define debounce_cycles 20

// Help to debug, generate a unique identifier for the event generating tasks
#define BUTTON_PROTO_ID 100

typedef enum pin {
    A = 12,
    B = 13,
    X = 14,
    Y = 15,
} pin_t;

void pico_buttons_init() {
    // setup button inputs
    gpio_set_function(A, GPIO_FUNC_SIO);
    gpio_set_dir(A, GPIO_IN);
    gpio_pull_up(A);
    gpio_set_function(B, GPIO_FUNC_SIO);
    gpio_set_dir(B, GPIO_IN);
    gpio_pull_up(B);
    gpio_set_function(X, GPIO_FUNC_SIO);
    gpio_set_dir(X, GPIO_IN);
    gpio_pull_up(X);
    gpio_set_function(Y, GPIO_FUNC_SIO);
    gpio_set_dir(Y, GPIO_IN);
    gpio_pull_up(Y);
}

bool is_pressed(uint8_t button) { return !gpio_get(button); }

void pico_buttons_task(__unused void *params) {
    uint16_t debounceA = 0;
    uint16_t debounceB = 0;
    uint16_t debounceX = 0;
    uint16_t debounceY = 0;

    while (true) {
        if (debounceA == 0 && is_pressed(A)) {
            event_t *event = create_event(EVENT_TYPE_NOTIFICATION,
                                          EVENT_BUTTON_A_PRESSED, NULL, 0);
            if (event) {
                event->proto_source = BUTTON_PROTO_ID;
                event_dispatcher_post(event);
            }
            debounceA = debounce_cycles;
        } else if (debounceA > 0) {
            debounceA--;
        }

        if (debounceB == 0 && is_pressed(B)) {
            event_t *event = create_event(EVENT_TYPE_NOTIFICATION,
                                          EVENT_BUTTON_B_PRESSED, NULL, 0);
            if (event) {
                event->proto_source = BUTTON_PROTO_ID;
                event_dispatcher_post(event);
            }
            debounceB = debounce_cycles;
        } else if (debounceB > 0) {
            debounceB--;
        }

        if (debounceX == 0 && is_pressed(X)) {
            event_t *event = create_event(EVENT_TYPE_NOTIFICATION,
                                          EVENT_BUTTON_X_PRESSED, NULL, 0);
            if (event) {
                event->proto_source = BUTTON_PROTO_ID;
                event_dispatcher_post(event);
            }
            debounceX = debounce_cycles;
        } else if (debounceX > 0) {
            debounceX--;
        }

        if (debounceY == 0 && is_pressed(Y)) {
            event_t *event = create_event(EVENT_TYPE_NOTIFICATION,
                                          EVENT_BUTTON_Y_PRESSED, NULL, 0);
            if (event) {
                event->proto_source = BUTTON_PROTO_ID;
                event_dispatcher_post(event);
            }
            debounceY = debounce_cycles;
        } else if (debounceY > 0) {
            debounceY--;
        }

        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

void start_buttons_task(void) {
    pico_buttons_init();

    xTaskCreate(pico_buttons_task, "pico_btns_task",
                PICO_BUTTONS_TASK_STACK_SIZE, NULL, PICO_BUTTONS_TASK_PRIORITY,
                NULL);
}

QueueHandle_t unicorn_event_queue;

void unicorn_task_init() {
    pico_unicorn_init();
    printf("PicoUnicorn initialized\n");

    unicorn_event_queue = xQueueCreate(10, sizeof(event_t *));
    event_dispatcher_register(unicorn_event_queue, EVENT_TYPE_NOTIFICATION,
                              EVENT_BUTTON_A_PRESSED);
    event_dispatcher_register(unicorn_event_queue, EVENT_TYPE_NOTIFICATION,
                              EVENT_BUTTON_B_PRESSED);
    event_dispatcher_register(unicorn_event_queue, EVENT_TYPE_NOTIFICATION,
                              EVENT_BUTTON_X_PRESSED);
    event_dispatcher_register(unicorn_event_queue, EVENT_TYPE_NOTIFICATION,
                              EVENT_BUTTON_Y_PRESSED);
}

void unicorn_task(__unused void *params) {
    event_t *event = NULL;
    bool button_a = false;
    bool button_b = false;
    bool button_x = false;
    bool button_y = false;

    int i = 0;
    while (true) {
        if (xQueueReceive(unicorn_event_queue, &event, pdMS_TO_TICKS(10)) ==
            pdTRUE) {
            printf("PicoUnicorn received event");
            printf("Event description: type=%d subtype=%d\n", event->type,
                   event->subtype);

            if (event->type == EVENT_TYPE_NOTIFICATION) {
                if (event->subtype == EVENT_BUTTON_A_PRESSED) {
                    button_a = !button_a;
                } else if (event->subtype == EVENT_BUTTON_B_PRESSED) {
                    button_b = !button_b;
                } else if (event->subtype == EVENT_BUTTON_X_PRESSED) {
                    button_x = !button_x;
                } else if (event->subtype == EVENT_BUTTON_Y_PRESSED) {
                    button_y = !button_y;
                }
            }

            free_event(event); // Free the event memory
        }

        i++;

        float pulse = fmod(((float)i) / 20.0f, M_PI * 2.0f);
        int v = (int)((sin(pulse) * 50.0f) + 50.0f);

        pico_unicorn_clear();
        for (int y = 0; y < 7; y++) {
            for (int x = 0; x < 16; x++) {
                int v = (x + y + (i / 100)) % 2 == 0 ? 0 : 100;
                pico_unicorn_set_pixel(x, y, v);
            }
        }

        if (button_a) {
            pico_unicorn_set_pixel_rgb(0, 0, 255, 0, 0);
            pico_unicorn_set_pixel_rgb(0, 1, 255, 0, 0);
            pico_unicorn_set_pixel_rgb(1, 0, 255, 0, 0);
            pico_unicorn_set_pixel_rgb(1, 1, 255, 0, 0);
            pico_unicorn_set_pixel_rgb(1, 2, 255 / 2, 0, 0);
            pico_unicorn_set_pixel_rgb(0, 2, 255 / 2, 0, 0);
            pico_unicorn_set_pixel_rgb(2, 0, 255 / 2, 0, 0);
            pico_unicorn_set_pixel_rgb(2, 1, 255 / 2, 0, 0);
        }

        if (button_b) {
            pico_unicorn_set_pixel_rgb(0, 6, 0, 255, 0);
            pico_unicorn_set_pixel_rgb(0, 5, 0, 255, 0);
            pico_unicorn_set_pixel_rgb(1, 6, 0, 255, 0);
            pico_unicorn_set_pixel_rgb(1, 5, 0, 255, 0);
            pico_unicorn_set_pixel_rgb(1, 4, 0, 255 / 2, 0);
            pico_unicorn_set_pixel_rgb(0, 4, 0, 255 / 2, 0);
            pico_unicorn_set_pixel_rgb(2, 6, 0, 255 / 2, 0);
            pico_unicorn_set_pixel_rgb(2, 5, 0, 255 / 2, 0);
        }

        if (button_x) {
            pico_unicorn_set_pixel_rgb(15, 0, 0, 0, 255);
            pico_unicorn_set_pixel_rgb(15, 1, 0, 0, 255);
            pico_unicorn_set_pixel_rgb(14, 0, 0, 0, 255);
            pico_unicorn_set_pixel_rgb(14, 1, 0, 0, 255);
            pico_unicorn_set_pixel_rgb(14, 2, 0, 0, 255 / 2);
            pico_unicorn_set_pixel_rgb(15, 2, 0, 0, 255 / 2);
            pico_unicorn_set_pixel_rgb(13, 0, 0, 0, 255 / 2);
            pico_unicorn_set_pixel_rgb(13, 1, 0, 0, 255 / 2);
        }

        if (button_y) {
            pico_unicorn_set_pixel_rgb(15, 6, 255, 0, 255);
            pico_unicorn_set_pixel_rgb(15, 5, 255, 0, 255);
            pico_unicorn_set_pixel_rgb(14, 6, 255, 0, 255);
            pico_unicorn_set_pixel_rgb(14, 5, 255, 0, 255);
            pico_unicorn_set_pixel_rgb(14, 4, 255 / 2, 0, 255 / 2);
            pico_unicorn_set_pixel_rgb(15, 4, 255 / 2, 0, 255 / 2);
            pico_unicorn_set_pixel_rgb(13, 6, 255 / 2, 0, 255 / 2);
            pico_unicorn_set_pixel_rgb(13, 5, 255 / 2, 0, 255 / 2);
        }
    }
}

void start_unicorn_task(void) {
    unicorn_task_init();

    xTaskCreate(unicorn_task, "unicorn_task", UNICORN_TASK_STACK_SIZE, NULL,
                UNICORN_TASK_PRIORITY, NULL);
}

QueueHandle_t scroll_event_queue;

void scroll_task_init() {
    pico_scroll_init();
    printf("PicoScroll initialized\n");

    scroll_event_queue = xQueueCreate(10, sizeof(event_t *));
    event_dispatcher_register(scroll_event_queue, EVENT_TYPE_NOTIFICATION,
                              EVENT_SUBTYPE_NETWORK_UP);
    event_dispatcher_register(scroll_event_queue, EVENT_TYPE_NOTIFICATION,
                              EVENT_SUBTYPE_NETWORK_DOWN);

    event_dispatcher_register(scroll_event_queue, EVENT_TYPE_NOTIFICATION,
                              NOTIFICATION_NEIGHBOR_UP);
    event_dispatcher_register(scroll_event_queue, EVENT_TYPE_NOTIFICATION,
                              NOTIFICATION_NEIGHBOR_DOWN);

    event_dispatcher_register(scroll_event_queue, EVENT_TYPE_REQUEST,
                              REQUEST_PRINT);
    event_dispatcher_register(scroll_event_queue, EVENT_TYPE_REQUEST,
                              REQUEST_SHOW);
}

void scroll_task(__unused void *params) {

    pico_scroll_clear();
    pico_scroll_update();
    pico_scroll_scroll_text("Demo ON!", 255, 25);

    printf("PicoScroll Started\n");

    event_t *event = NULL;

    char text[256];

    while (true) {
        printf("PicoScroll waiting for events...\n");
        memset(text, 0, 256); // Clear text buffer
        if (xQueueReceive(scroll_event_queue, &event, portMAX_DELAY) ==
            pdTRUE) {
            printf("PicoScroll received event");
            printf("Event description: type=%d subtype=%d\n", event->type,
                   event->subtype);
            if (event->type == EVENT_TYPE_NOTIFICATION) {
                if (event->subtype == EVENT_SUBTYPE_NETWORK_UP) {
                    printf("Network: %s\n",
                           ((network_event_t *)event->payload)->ssid);
                    printf("IP: %s\n", ((network_event_t *)event->payload)->ip);
                    sprintf(text, "Network UP: %s %s",
                            ((network_event_t *)event->payload)->ssid,
                            ((network_event_t *)event->payload)->ip);
                    pico_scroll_scroll_text(text, 255, 25);
                } else if (event->subtype == EVENT_SUBTYPE_NETWORK_DOWN) {
                    pico_scroll_scroll_text("Network DOWN", 255, 25);
                } else if (event->subtype == NOTIFICATION_NEIGHBOR_UP) {
                    printf("Neighbor up: %s",
                           uuid_to_string((uint8_t *)event->payload));
                    sprintf(text, "Neighbor up: %s",
                            uuid_to_string((uint8_t *)event->payload));
                    pico_scroll_scroll_text(text, 255, 10);
                } else if (event->subtype == NOTIFICATION_NEIGHBOR_DOWN) {
                    printf("Neighbor down: %s",
                           uuid_to_string((uint8_t *)event->payload));
                    sprintf(text, "Neighbor down: %s",
                            uuid_to_string((uint8_t *)event->payload));
                    pico_scroll_scroll_text(text, 255, 10);
                }
            } else if (event->type == EVENT_TYPE_REQUEST) {
                if (event->subtype == REQUEST_PRINT) {
                    memcpy(text, event->payload,
                           strlen((char *)event->payload));
                    pico_scroll_scroll_text(text, 255, 20);
                } else if (event->subtype == REQUEST_SHOW) {
                    memcpy(text, event->payload,
                           strlen((char *)event->payload));
                    pico_scroll_set_text(text, 255);
                }
            }

            free_event(event);
        }
    }
}

void start_scroll_task(void) {
    scroll_task_init();

    xTaskCreate(scroll_task, "scroll_task", SCROLL_TASK_STACK_SIZE, NULL,
                SCROLL_TASK_PRIORITY, NULL);
}
