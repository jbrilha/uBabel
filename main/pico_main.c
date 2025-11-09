
#include <math.h>
#include <pico/time.h>
#include <stdio.h>
#include <string.h>

#include "event_dispatcher.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "spi_hal.h"
#include "platform.h"

#include "comm_manager.h"
#include "common_events.h"
#include "event_dispatcher.h"
#include "network_events.h"
#include "network_manager.h"
#include "pico_tasks.h"
#include "proto_iot_control.h"
#include "proto_simple_overlay.h"

#include "mem_check.h"

#include "proto_iot_control.h"
#include "app.h"
#include "spi_hal/spi_hal.h"

static const char *TAG = "PICO_MAIN";

// Which core to run on if configNUMBER_OF_CORES==1
#ifndef RUN_FREE_RTOS_ON_CORE
#define RUN_FREE_RTOS_ON_CORE 0
#endif

// Priorities of our threads - higher numbers are higher priority
#define MAIN_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)
#define MAIN_APP_PRIORITY (tskIDLE_PRIORITY + 2UL)
#define WORKER_TASK_PRIORITY (tskIDLE_PRIORITY + 4UL)

// Stack sizes of our threads in words (4 bytes)
#define MAIN_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define WORKER_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

void main_task(__unused void *params) {
    vTaskDelay(pdMS_TO_TICKS(10000)); // 10 second delay to connect terminal

    xTaskCreate(mem_check_task, // Task function
                "mem_check",    // Task name
                4096,           // Stack size in words
                NULL,           // Task parameters
                5,              // Priority
                NULL            // Task handle
    );

    event_dispatcher_init();
    comm_manager_init();

    start_unicorn_task();
    start_scroll_task();
    start_buttons_task();

    spi_hal_master_init();

    xTaskCreate(network_manager_task, // Task function
                "NetworkManager",     // Task name
                4096,                 // Stack size in words
                NULL,                 // Task parameters
                5,                    // Priority
                NULL                  // Task handle
    );

    simple_overlay_network_init();
    iot_control_protocol_init();

    application_init();

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
