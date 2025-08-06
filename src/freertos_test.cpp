#include "pico_unicorn.hpp" 
#include "pico_scroll.hpp" // cpp headers first!!

#include "network_events.h"
#include "network_manager.h"
#include "event_dispatcher.h"

#include <pico/time.h>
#include <stdio.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/dns.h"
#include "lwip/ip4_addr.h"
#include "lwip/netdb.h"
#include "lwip/pbuf.h"
#include "lwip/sockets.h"
#include "lwip/udp.h"

#include "lwip/netdb.h"
#include "lwip/sockets.h"


#define WIFI_SSID "ESP32_AP"
#define WIFI_PASS "superSafeAP"
#define ESP32_IP "192.168.4.1" // default

#define UDP_PORT 8080

// Which core to run on if configNUMBER_OF_CORES==1
#ifndef RUN_FREE_RTOS_ON_CORE
#define RUN_FREE_RTOS_ON_CORE 0
#endif

// Priorities of our threads - higher numbers are higher priority
#define MAIN_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)
#define SCROLL_TASK_PRIORITY (tskIDLE_PRIORITY + 1UL)
#define UNICORN_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)
#define WORKER_TASK_PRIORITY (tskIDLE_PRIORITY + 4UL)

// Stack sizes of our threads in words (4 bytes)
#define MAIN_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define SCROLL_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define UNICORN_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define WORKER_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

using namespace pimoroni;
PicoScroll pico_scroll;
PicoUnicorn pico_unicorn;

void udp_receiver_task(void *pvParameters) {
    printf("UDP receiver task started\n");
    
    int sock = *(int*)pvParameters;

    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    char buffer[256];
    
    int len;
    while (true) {
        len = lwip_recvfrom(sock, buffer, sizeof(buffer)-1, 0,
                           (struct sockaddr*)&sender_addr, &sender_len);
        
        if (len > 0) {
            buffer[len] = '\0';
            char sender_ip[16];
            strcpy(sender_ip, inet_ntoa(sender_addr.sin_addr));
            printf("received from %s: %s\n", sender_ip, buffer);
        } else if (len < 0) {
            printf("recvfrom error: %d\n", len);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    lwip_close(sock);
}

void udp_sender_task(void *pvParameters) {
    printf("UDP sender task started\n");
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    int sock = *(int*)pvParameters;
    struct sockaddr_in server_addr;
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    inet_aton(ESP32_IP, &server_addr.sin_addr);
    
    int msg_count = 0;
    while (true) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Hello from Pico #%d", msg_count++);
        
        int sent = lwip_sendto(sock, msg, strlen(msg), 0,
                              (struct sockaddr*)&server_addr, sizeof(server_addr));
        
        if (sent > 0) {
            printf("sent: %s\n", msg);
        } else {
            printf("send failed: %d\n", sent);
        }
        
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    
    lwip_close(sock);
}


void udp_task(void *pvParameters) {
    printf("UDP task started\n");
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    int sock;
    struct sockaddr_in local_addr, server_addr, sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    char buffer[256];
    
    sock = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("failed to create socket\n");
        vTaskDelete(NULL);
        return;
    }
    
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(8080);
    
    if (lwip_bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        printf("failed to bind socket\n");
        lwip_close(sock);
        vTaskDelete(NULL);
        return;
    }
    
    printf("Pico bound to port 8080\n");
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    inet_aton("192.168.4.1", &server_addr.sin_addr);
    
    xTaskCreate(udp_sender_task, "udp_tx", 1024, &sock, WORKER_TASK_PRIORITY, NULL);
    xTaskCreate(udp_receiver_task, "udp_rx", 1024, &sock, WORKER_TASK_PRIORITY, NULL);
    
    vTaskDelete(NULL);
}

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
    
    xTaskCreate(udp_task, "udp_task", 4096, NULL, WORKER_TASK_PRIORITY, NULL);
    
    // this task is done, can kill itself 
    vTaskDelete(NULL);
}

void unicorn_task(__unused void *params) {
    pico_unicorn.init();
    printf("PicoUnicorn initialized\n");
    
    int i = 0;
    while(true) {
        i++;
        pico_unicorn.clear();
        for(int y = 0; y < 7; y++) {
            for(int x = 0; x < 16; x++) {
            int v = (x + y + (i / 100)) % 2 == 0 ? 0 : 100;
            pico_unicorn.set_pixel(x, y, v);
            }
        }

        //pico_unicorn.update();
        sleep_ms(10);
    }
}

QueueHandle_t scroll_event_queue;

void scroll_task_init() {
    pico_scroll.init();
    printf("PicoScroll initialized\n");

    scroll_event_queue = xQueueCreate(10, sizeof(event_t*));
    event_dispatcher_register(scroll_event_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_UP);
    event_dispatcher_register(scroll_event_queue, EVENT_TYPE_NOTIFICATION, EVENT_SUBTYPE_NETWORK_DOWN);
}

void scroll_task(__unused void *params) {

    pico_scroll.clear();
    pico_scroll.update();
    pico_scroll.scroll_text("Demo ON!", 255, 100);

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
                    pico_scroll.scroll_text(text, 255, 100 );
                } else if(event->subtype == EVENT_SUBTYPE_NETWORK_DOWN) {
                    pico_scroll.scroll_text("Network DOWN", 255, 100);
                }
                if(event->payload != NULL)
                    free(event->payload); // Free the payload memory
                free(event);
            }
        }
    }
}

void main_task(__unused void *params) {

    event_dispatcher_init();
    printf("Event dispatcher initialized\n");

    scroll_task_init();

    xTaskCreate(scroll_task, "scroll_task", SCROLL_TASK_STACK_SIZE, NULL,
                SCROLL_TASK_PRIORITY, NULL);

    xTaskCreate(unicorn_task, "unicorn_task", UNICORN_TASK_STACK_SIZE, NULL,
                UNICORN_TASK_PRIORITY, NULL);

    //xTaskCreate(wifi_connect_task, "wifi_task", 2048, NULL, WORKER_TASK_PRIORITY, NULL);

    xTaskCreate(
        network_manager_task,   // Task function
        "NetworkManager",       // Task name
        4096,                   // Stack size in words
        NULL,                   // Task parameters
        1,                      // Priority
        NULL                    // Task handle
    );

    vTaskDelete(NULL);
}

void vLaunch(void) {
    sleep_ms(10000);

    printf("FreeRTOS starting...\n");

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
