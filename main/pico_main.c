
#include <pico/time.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "event_dispatcher.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "platform.h"

#include "lwip/dns.h"
#include "lwip/ip4_addr.h"
#include "lwip/netdb.h"
#include "lwip/pbuf.h"
#include "lwip/sockets.h"
#include "lwip/udp.h"

#include "network_events.h"
#include "network_manager.h"
#include "event_dispatcher.h"
#include "pico_buttons.h"

#include "tcp.h"
#include "udp.h"

#include "pico_scroll_wrapper.h"
#include "pico_unicorn_wrapper.h"

#define WIFI_SSID "ESP32_AP"
#define WIFI_PASS "superSafeAP"
#define ESP32_IP "192.168.4.1" // default

// Which core to run on if configNUMBER_OF_CORES==1
#ifndef RUN_FREE_RTOS_ON_CORE
#define RUN_FREE_RTOS_ON_CORE 0
#endif

// Priorities of our threads - higher numbers are higher priority
#define MAIN_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)
#define SCROLL_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)
#define UNICORN_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)
#define WORKER_TASK_PRIORITY (tskIDLE_PRIORITY + 4UL)

// Stack sizes of our threads in words (4 bytes)
#define MAIN_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define SCROLL_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define UNICORN_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define WORKER_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

void wifi_connect_task(void *pvParameters) {
    printf("Wi-Fi task started\n");

    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        vTaskDelete(NULL);
        return;
    }

    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS,
                                           CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect to Wi-Fi\n");
        vTaskDelete(NULL);
        return;
    }

    printf("connected to AP\n");
    printf("IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));

    xTaskCreate(udp_client_task, "udp_client", 2048, NULL, WORKER_TASK_PRIORITY, NULL);
    xTaskCreate(tcp_client_task, "tcp_client", 2048, NULL, WORKER_TASK_PRIORITY, NULL);

    // this task is done, can kill itself
    vTaskDelete(NULL);
}

QueueHandle_t scroll_event_queue;

void scroll_task_init() {
    pico_scroll_init();
    printf("PicoScroll initialized\n");

    scroll_event_queue = xQueueCreate(10, sizeof(event_t*));
    event_dispatcher_register(scroll_event_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_UP);
    event_dispatcher_register(scroll_event_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_DOWN);
}

void scroll_task(__unused void *params) {

    pico_scroll_clear();
    pico_scroll_update();
    pico_scroll_scroll_text("Demo ON!", 255, 100);

    printf("PicoScroll Started\n");

    event_t* event = NULL;

    char text[256];

    while (true) {
        printf("PicoScroll waiting for events...\n");
        if(xQueueReceive(scroll_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            printf("PicoScroll received event");
            printf("Event description: type=%d subtype=%d\n", event->type, event->subtype);
            if(event->type == EVENT_TYPE_NOTIFICATION) {
                if(event->subtype == EVENT_SUBTYPE_NETWORK_UP) {
                    memset(text, 0, 256); // Clear text buffer
                    printf("Network: %s\n", ((network_event_t*)event->payload)->ssid);
                    printf("IP: %s\n", ((network_event_t*)event->payload)->ip);
                    
                    sprintf(text, "Network UP: %s %s", ((network_event_t*)event->payload)->ssid, ((network_event_t*)event->payload)->ip);
                    pico_scroll_scroll_text(text, 255, 100 );
                } else if(event->subtype == EVENT_SUBTYPE_NETWORK_DOWN) {
                    pico_scroll_scroll_text("Network DOWN", 255, 100);
                }
                if(event->payload != NULL)
                    free(event->payload); // Free the payload memory
                free(event);
            }
        }
    }
}


QueueHandle_t unicorn_event_queue;

void unicorn_task_init() {
    pico_unicorn_init();
    printf("PicoUnicorn initialized\n");

    unicorn_event_queue = xQueueCreate(10, sizeof(event_t*));
    event_dispatcher_register(unicorn_event_queue, EVENT_TYPE_NOTIFICATION, EVENT_BUTTON_A_PRESSED);
    event_dispatcher_register(unicorn_event_queue, EVENT_TYPE_NOTIFICATION, EVENT_BUTTON_B_PRESSED);
    event_dispatcher_register(unicorn_event_queue, EVENT_TYPE_NOTIFICATION, EVENT_BUTTON_X_PRESSED);
    event_dispatcher_register(unicorn_event_queue, EVENT_TYPE_NOTIFICATION, EVENT_BUTTON_Y_PRESSED);    
}

void unicorn_task(__unused void *params) {
    event_t* event = NULL;
    bool button_a = false;
    bool button_b = false;
    bool button_x = false;
    bool button_y = false;

    int i = 0;
    while(true) {
        if(xQueueReceive(unicorn_event_queue, &event, pdMS_TO_TICKS(20)) == pdTRUE) {
            printf("PicoUnicorn received event");
            printf("Event description: type=%d subtype=%d\n", event->type, event->subtype);

            if(event->type == EVENT_TYPE_NOTIFICATION) {
                if(event->subtype == EVENT_BUTTON_A_PRESSED) {
                    button_a = !button_a;   
                } else if(event->subtype == EVENT_BUTTON_B_PRESSED) {
                    button_b = !button_b;
                } else if(event->subtype == EVENT_BUTTON_X_PRESSED) {
                    button_x = !button_x;
                } else if(event->subtype == EVENT_BUTTON_Y_PRESSED) {
                    button_y = !button_y;
                }
                free(event); // Free the event memory
            }
        } 

        i++;
            
        float pulse = fmod(((float)i) / 20.0f, M_PI * 2.0f);
        int v = (int)((sin(pulse) * 50.0f) + 50.0f);

        pico_unicorn_clear();
        for(int y = 0; y < 7; y++) {
            for(int x = 0; x < 16; x++) {
            int v = (x + y + (i / 100)) % 2 == 0 ? 0 : 100;
            pico_unicorn_set_pixel(x, y, v);
            }
        }

        if(button_a) {
            pico_unicorn_set_pixel_rgb(0, 0, 255, 0, 0);
            pico_unicorn_set_pixel_rgb(0, 1, 255, 0, 0);
            pico_unicorn_set_pixel_rgb(1, 0, 255, 0, 0);
            pico_unicorn_set_pixel_rgb(1, 1, 255, 0, 0);
            pico_unicorn_set_pixel_rgb(1, 2, 255 / 2, 0, 0);
            pico_unicorn_set_pixel_rgb(0, 2, 255 / 2, 0, 0);
            pico_unicorn_set_pixel_rgb(2, 0, 255 / 2, 0, 0);
            pico_unicorn_set_pixel_rgb(2, 1, 255 / 2, 0, 0);
        }

        if(button_b) {
            pico_unicorn_set_pixel_rgb(0, 6, 0, 255, 0);
            pico_unicorn_set_pixel_rgb(0, 5, 0, 255, 0);
            pico_unicorn_set_pixel_rgb(1, 6, 0, 255, 0);
            pico_unicorn_set_pixel_rgb(1, 5, 0, 255, 0);
            pico_unicorn_set_pixel_rgb(1, 4, 0, 255 / 2, 0);
            pico_unicorn_set_pixel_rgb(0, 4, 0, 255 / 2, 0);
            pico_unicorn_set_pixel_rgb(2, 6, 0, 255 / 2, 0);
            pico_unicorn_set_pixel_rgb(2, 5, 0, 255 / 2, 0);
        }

        if(button_x) {
            pico_unicorn_set_pixel_rgb(15, 0, 0, 0, 255);
            pico_unicorn_set_pixel_rgb(15, 1, 0, 0, 255);
            pico_unicorn_set_pixel_rgb(14, 0, 0, 0, 255);
            pico_unicorn_set_pixel_rgb(14, 1, 0, 0, 255);
            pico_unicorn_set_pixel_rgb(14, 2, 0, 0, 255 / 2);
            pico_unicorn_set_pixel_rgb(15, 2, 0, 0, 255 / 2);
            pico_unicorn_set_pixel_rgb(13, 0, 0, 0, 255 / 2);
            pico_unicorn_set_pixel_rgb(13, 1, 0, 0, 255 / 2);
        }

        if(button_y) {
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


void main_task(__unused void *params) {
    event_dispatcher_init();
    printf("Event dispatcher initialized\n");

    scroll_task_init();
    unicorn_task_init();
    pico_buttons_init();

    xTaskCreate(scroll_task, "scroll_task", SCROLL_TASK_STACK_SIZE, NULL,
                SCROLL_TASK_PRIORITY, NULL);

    xTaskCreate(unicorn_task, "unicorn_task", UNICORN_TASK_STACK_SIZE, NULL,
                UNICORN_TASK_PRIORITY, NULL);

    xTaskCreate(pico_buttons_task, "buttons_task", configMINIMAL_STACK_SIZE, NULL,
                WORKER_TASK_PRIORITY, NULL);

    xTaskCreate(
        network_manager_task,   // Task function
        "NetworkManager",       // Task name
        4096,                   // Stack size in words
        NULL,                   // Task parameters
        5,                      // Priority
        NULL                    // Task handle
    );

    vTaskDelete(NULL);
}

void vLaunch(void) {
    TaskHandle_t task;
    xTaskCreate(main_task, "main_thread", MAIN_TASK_STACK_SIZE, NULL,
                MAIN_TASK_PRIORITY, &task);

#if configUSE_CORE_AFFINITY && configNUMBER_OF_CORES > 1
    // we must bind the main task to one core (well at least while the init is
    // called)
    vTaskCoreAffinitySet(task, 1);
#endif

    /* Start the tasks and timer running. */
    vTaskStartScheduler();
}

int main(void) {
    stdio_init_all();

    const char *rtos_name;
#if (configNUMBER_OF_CORES > 1)
    rtos_name = "FreeRTOS SMP";
#else
    rtos_name = "FreeRTOS";
#endif

#if (configNUMBER_OF_CORES > 1)
    printf("Starting %s on both cores:\n", rtos_name);
    vLaunch();
#elif (RUN_FREE_RTOS_ON_CORE == 1 && configNUMBER_OF_CORES == 1)
    printf("Starting %s on core 1:\n", rtos_name);
    multicore_launch_core1(vLaunch);
    while (true)
        ;
#else
    printf("Starting %s on core 0:\n", rtos_name);
    vLaunch();
#endif
    return 0;
}
