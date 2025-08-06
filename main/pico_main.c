
#include <pico/time.h>
#include <stdio.h>
#include <string.h>

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

#include "network_manager.h"
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
#define WORKER_TASK_PRIORITY (tskIDLE_PRIORITY + 4UL)

// Stack sizes of our threads in words (4 bytes)
#define MAIN_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define SCROLL_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
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

void scroll_task(__unused void *params) {
    pico_scroll_init();
    printf("PicoScroll initialized\n");

    pico_scroll_clear();
    pico_scroll_set_text("Hello World!", 255);
    pico_scroll_update();

    printf("PicoScroll ready\n");

    while (true) {
        printf("123\n");
        pico_scroll_scroll_text("123", 255, 100);
        sleep_ms(500);
        pico_scroll_clear();
        sleep_ms(500);
        printf("456\n");
        pico_scroll_scroll_text("456", 255, 100);
        pico_scroll_clear();
        sleep_ms(500);
        if(pico_scroll_is_pressed(PICO_SCROLL_A)) {
            printf("Button A pressed\n");
        }
    }
}

void unicorn_task(__unused void *params) {
    pico_unicorn_init();
    printf("PicoUnicorn initialized\n");
    
    int i = 0;
    while(true) {
        printf("aaa\n");
        i++;
        pico_unicorn_clear();
        for(int y = 0; y < 7; y++) {
            for(int x = 0; x < 16; x++) {
            int v = (x + y + (i / 100)) % 2 == 0 ? 0 : 100;
            pico_unicorn_set_pixel(x, y, v);
            }
        }

        //pico_unicorn.update();
        sleep_ms(1000);
    }
}


void main_task(__unused void *params) {
    // xTaskCreate(scroll_task, "scroll_task", MAIN_TASK_STACK_SIZE, NULL,
    //             MAIN_TASK_PRIORITY, NULL);

    xTaskCreate(unicorn_task, "unicorn_task", MAIN_TASK_STACK_SIZE, NULL,
                MAIN_TASK_PRIORITY, NULL);

    // xTaskCreate(wifi_connect_task, "wifi_task", 2048, NULL,
    //             WORKER_TASK_PRIORITY, NULL);

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
