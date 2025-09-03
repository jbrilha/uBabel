
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
#include "comm_manager.h"
#include "pico_buttons.h"
#include "common_events.h"
#include "proto_simple_overlay.h"
#include "proto_iot_control.h"

#include "tcp.h"
#include "udp.h"
#include "pico_touch_screen.h"
#include "mem_check.h"

#include "pico_scroll_wrapper.h"
#include "pico_unicorn_wrapper.h"

#include "proto_iot_control.h"

static const char *TAG = "PICO_MAIN";

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
#define MAIN_APP_PRIORITY (tskIDLE_PRIORITY + 2UL)
#define WORKER_TASK_PRIORITY (tskIDLE_PRIORITY + 4UL)

// Stack sizes of our threads in words (4 bytes)
#define MAIN_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define SCROLL_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define UNICORN_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define WORKER_TASK_STACK_SIZE configMINIMAL_STACK_SIZE

#define REQUEST_PRINT 800
#define REQUEST_SHOW  801

static void scroll_node(iot_node_handle_t device) {
    static char device_id[16*2+5];
    memset(device_id, 0, 16*2+5);

    char* text = (char*) malloc(256);

    if(text != NULL) {
        memset(text, 0, 256);
        printf("Going to print the node identifier to a textual representation\n");
        print_node_identifier(device, device_id);
        printf("Got the node identifier %s\n", device_id);
        sprintf(text, "Current device: %s", device_id);
        event_t* print_event = create_event(EVENT_TYPE_REQUEST, REQUEST_PRINT, text, 256);
        
        if(print_event == NULL || !event_dispatcher_post(print_event)) {
            if(print_event != NULL) {
                free_event(print_event);
            } else {
                free(text);
            }
        }
    }
}

static void show_device(iot_node_handle_t node_handle, iot_device_handle_t device_handle, device_t* device_info) {
    
    char* text = (char*) malloc(9);

    if(text != NULL) {
        memset(text,0, 9);
        LOG_INFO(TAG, "Going to print a device with device_handle %d", device_handle);
        if(device_handle < 0 || device_info == NULL) {
            switch(device_handle) {
            case NO_DEVICE:
                sprintf(text, "NO DEV");
                break;
            case INVALID_NODE:
            default:  
                sprintf(text, "ERR NODE");
                break;      
            }        
        } else {
            LOG_INFO(TAG, "Going to print a device with device type %d", device_info->device_type);
            free(text);
            text = strdup(device_info->device_name);
            if(text == NULL) {
                LOG_ERROR(TAG, "Failed to allocate memory for device name");
                return;
            }      
        }
        
        event_t* print_event = create_event(EVENT_TYPE_REQUEST, REQUEST_PRINT, text, 8);
        if(print_event == NULL || !event_dispatcher_post(print_event)) {
            if(print_event != NULL) {
                free_event(print_event);
            } else {
                free(text);
            }
        }
    }
}

typedef enum {
    node,
    device,
    action,
    parameter
} navigation_mode_t;

static QueueHandle_t application_queue;
static iot_node_handle_t node_handle;
static iot_device_handle_t device_handle;
static navigation_mode_t nav;

static const device_t* device_info; //From IoTControl protocol  
static device_t* current_device = NULL;
static action_t* current_action = NULL; 
static parameter_t* current_parameter = NULL;

static device_t* find_correct_device(uint8_t device_type) {
    device_t* current = (device_t*) device_info;
    while(current != NULL) {
        if(current->device_type == device_type) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static void show_error(const char* message) {
    if(message != NULL) {
        char* text = strdup(message);
        if(text != NULL) {
            event_t* print_event = create_event(EVENT_TYPE_REQUEST, REQUEST_PRINT, text, strlen(text));
            if(print_event == NULL || !event_dispatcher_post(print_event)) {
                if(print_event != NULL) {
                    free_event(print_event);
                } else {
                    free(text);
                }
            }
        }
    }
}   

static void show_action(action_t* action_info) {
    char* text = NULL;
    
    if(action_info != NULL) {
        text = strdup(action_info->action_name);        
    } else {
        text = strdup("No action selected");
    }

    if(text != NULL) {
        event_t* print_event = create_event(EVENT_TYPE_REQUEST, REQUEST_PRINT, text, strlen(text));
        if(print_event == NULL || !event_dispatcher_post(print_event)) {
            if(print_event != NULL) {
                free_event(print_event);
            } else {
                free(text);
            }
        }
    }
}

static void show_parameter(parameter_t* parameter) {
    char* text = NULL;

    if(parameter != NULL) {
        text = strdup(parameter->parameter_name);
    } else {
        text = strdup("Action does not have parameters");
    }

    if(text != NULL) {
        event_t* print_event = create_event(EVENT_TYPE_REQUEST, REQUEST_PRINT, text, strlen(text));
        if(print_event == NULL || !event_dispatcher_post(print_event)) {
            if(print_event != NULL) {
                free_event(print_event);
            } else {
                free(text);
            }
        }
    }
}

void application_task(void *pvParameters) {
    char output[256];
    event_t* event = NULL;

    while (true) {
        memset(output, 0, 256); // Clear text buffer
        if(xQueueReceive(application_queue, &event, portMAX_DELAY) == pdTRUE) {
            printf("Application main loop has received event: type=%d subtype=%d\n", event->type, event->subtype);

            if(event->type == EVENT_TYPE_NOTIFICATION) {

                if(event->subtype == EVENT_BUTTON_A_PRESSED) {
                    //go up a level or execute
                    switch(nav) {
                        case node:
                            nav = device;
                            device_handle = initialize_device_iterator(node_handle);
                            uint16_t device_typology = get_device_type(node_handle, device_handle);
                            current_device = find_correct_device(device_typology);
                            printf("Main Action, device handle is: %d\n", device_handle);
                            show_device(node_handle, device_handle, current_device);
                            break;
                        case device:
                            if(current_device == NULL) {
                                nav = device;
                                uint16_t device_typology = get_device_type(node_handle, device_handle);
                                current_device = find_correct_device(device_typology);
                                show_error("No valid device");
                            } else {
                                nav = action;
                                current_action = current_device->actions;
                                show_action(current_action);
                            }
                            break;
                        case action:
                            if(current_device == NULL) {
                                nav = device;
                                uint16_t device_typology = get_device_type(node_handle, device_handle);
                                current_device = find_correct_device(device_typology);
                                show_error("No valid device");
                            } else if(current_action == NULL) {
                                show_error("No valid action");  
                            } else {
                                nav = parameter;
                                current_parameter = current_action->parameters;
                                show_parameter(current_parameter);
                            }
                            break;
                        case parameter:
                            //This is to effectively execute the action over the device using the appropriate parameters
                            device_action(node_handle, device_handle, current_device, current_action, current_parameter);
                            break;
                        default:
                            LOG_ERROR(TAG, "Unknown navigation mode");
                            break;
                    }

                } else if(event->subtype == EVENT_BUTTON_B_PRESSED) {
                    //Previous element in the list
                    switch(nav) {
                        case node:
                            node_handle = previous_node(node_handle);
                            scroll_node(node_handle);    
                            break;
                        case device:
                            device_handle = previous_device(node_handle, device_handle);
                            uint16_t device_typology = get_device_type(node_handle, device_handle);
                            current_device = find_correct_device(device_typology);
                            printf("Main Action, device handle is: %d\n", device_handle);
                            show_device(node_handle, device_handle, current_device);
                            break;
                        case action:
                            if(current_action != NULL) {
                                current_action = current_action->prev;
                                show_action(current_action);
                            } else {
                                nav = device;
                                show_error("No action available");
                            }
                            break;
                        case parameter:
                            if(current_parameter != NULL) {
                                current_parameter = current_parameter->prev;
                                show_parameter(current_parameter);
                            } else {
                                show_error("No parameter available");
                            }
                            break;
                        default:
                            LOG_ERROR(TAG, "Unknown navigation mode");
                            break;
                    }

                } else if(event->subtype == EVENT_BUTTON_X_PRESSED) {
                    //Go down a level (or in node go to broadcast)
                    switch(nav) {
                        case node:
                            //nothing to be done
                        case device:
                            nav = node;
                            if(node_handle == NULL)
                                node_handle = initialize_node_iterator();
                            scroll_node(node_handle);
                            break;
                        case action:
                            nav = device;
                            show_device(node_handle, device_handle, current_device);
                            break;
                        case parameter:
                            nav = action;
                            show_action(current_action);
                            break;
                        default:
                            LOG_ERROR(TAG, "Unknown navigation mode");
                            break;
                    }

                } else if(event->subtype == EVENT_BUTTON_Y_PRESSED) {
                    //Next element on the list
                    switch(nav) {
                        case node:
                            node_handle = previous_node(node_handle);
                            scroll_node(node_handle);    
                            break;
                        case device:
                            device_handle = next_device(node_handle, device_handle);
                            uint16_t device_typology = get_device_type(node_handle, device_handle);
                            current_device = find_correct_device(device_typology);
                            printf("Main Action, device handle is: %d\n", device_handle);
                            show_device(node_handle, device_handle, current_device);
                            break;
                        case action:
                            if(current_action != NULL) {
                                current_action = current_action->next;
                                show_action(current_action);
                            } else {
                                nav = device;
                                show_error("No action available");
                            }
                            break;
                        case parameter:
                            if(current_parameter != NULL) {
                                current_parameter = current_parameter->prev;
                                show_parameter(current_parameter);
                            } else {
                                show_error("No parameter available");
                            }
                            break;
                        default:
                            LOG_ERROR(TAG, "Unknown navigation mode");
                            break;
                    }
                }
            }
            free_event(event);
            event = NULL;
        }
    }
}

void application_init() {
    application_queue = xQueueCreate(10, sizeof(event_t*));
    event_dispatcher_register(application_queue, EVENT_TYPE_NOTIFICATION, EVENT_BUTTON_A_PRESSED);
    event_dispatcher_register(application_queue, EVENT_TYPE_NOTIFICATION, EVENT_BUTTON_B_PRESSED);
    event_dispatcher_register(application_queue, EVENT_TYPE_NOTIFICATION, EVENT_BUTTON_X_PRESSED);
    event_dispatcher_register(application_queue, EVENT_TYPE_NOTIFICATION, EVENT_BUTTON_Y_PRESSED); 

    nav = node;
    device_info = get_device_info_data();

    node_handle = initialize_node_iterator();

    xTaskCreate(application_task, "main_app_task", configMINIMAL_STACK_SIZE, NULL,
                MAIN_APP_PRIORITY, NULL);
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

    event_dispatcher_register(scroll_event_queue, EVENT_TYPE_NOTIFICATION, NOTIFICATION_NEIGHBOR_UP);
    event_dispatcher_register(scroll_event_queue, EVENT_TYPE_NOTIFICATION, NOTIFICATION_NEIGHBOR_DOWN);

    event_dispatcher_register(scroll_event_queue, EVENT_TYPE_REQUEST, REQUEST_PRINT);
    event_dispatcher_register(scroll_event_queue, EVENT_TYPE_REQUEST, REQUEST_SHOW);
}

void scroll_task(__unused void *params) {

    pico_scroll_clear();
    pico_scroll_update();
    pico_scroll_scroll_text("Demo ON!", 255, 25);

    printf("PicoScroll Started\n");

    event_t* event = NULL;

    char text[256];

    while (true) {
        printf("PicoScroll waiting for events...\n");
        memset(text, 0, 256); // Clear text buffer
        if(xQueueReceive(scroll_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            printf("PicoScroll received event");
            printf("Event description: type=%d subtype=%d\n", event->type, event->subtype);
            if(event->type == EVENT_TYPE_NOTIFICATION) {
                if(event->subtype == EVENT_SUBTYPE_NETWORK_UP) {              
                    printf("Network: %s\n", ((network_event_t*)event->payload)->ssid);
                    printf("IP: %s\n", ((network_event_t*)event->payload)->ip);       
                    sprintf(text, "Network UP: %s %s", ((network_event_t*)event->payload)->ssid, ((network_event_t*)event->payload)->ip);
                    pico_scroll_scroll_text(text, 255, 25 );
                } else if(event->subtype == EVENT_SUBTYPE_NETWORK_DOWN) {
                    pico_scroll_scroll_text("Network DOWN", 255, 25);
                } else if(event->subtype == NOTIFICATION_NEIGHBOR_UP) {
                    printf("Neighbor up: %s", uuid_to_string((uint8_t*) event->payload));
                    sprintf(text, "Neighbor up: %s", uuid_to_string((uint8_t*) event->payload));
                    pico_scroll_scroll_text(text, 255, 10);
                } else if(event->subtype == NOTIFICATION_NEIGHBOR_DOWN) {
                    printf("Neighbor down: %s", uuid_to_string((uint8_t*) event->payload));
                    sprintf(text, "Neighbor down: %s", uuid_to_string((uint8_t*) event->payload));
                    pico_scroll_scroll_text(text, 255, 10);
                }
            } else if (event->type == EVENT_TYPE_REQUEST) {
                if(event->subtype == REQUEST_PRINT) {
                    memcpy(text, event->payload, strlen((char*)event->payload));
                    pico_scroll_scroll_text(text, 255, 20);
                } else if (event->subtype == REQUEST_SHOW) {
                    memcpy(text, event->payload, strlen((char*) event->payload));
                    pico_scroll_set_text(text, 255);
                }
            }

            free_event(event);
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
        if(xQueueReceive(unicorn_event_queue, &event, pdMS_TO_TICKS(10)) == pdTRUE) {
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
            }

            free_event(event); // Free the event memory
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
    vTaskDelay(pdMS_TO_TICKS(10000)); //10 second delay to connect terminal

    xTaskCreate(
        mem_check_task,         // Task function
        "mem_check",            // Task name
        4096,                   // Stack size in words
        NULL,                   // Task parameters
        5,                      // Priority
        NULL                    // Task handle
    );

    event_dispatcher_init();
    comm_manager_init();

    scroll_task_init();
    unicorn_task_init();
    pico_buttons_init();

    xTaskCreate(scroll_task, "scroll_task", SCROLL_TASK_STACK_SIZE, NULL,
                SCROLL_TASK_PRIORITY, NULL);

    xTaskCreate(unicorn_task, "unicorn_task", UNICORN_TASK_STACK_SIZE, NULL,
                UNICORN_TASK_PRIORITY, NULL);

    xTaskCreate(pico_buttons_task, "buttons_task", configMINIMAL_STACK_SIZE, NULL,
                WORKER_TASK_PRIORITY, NULL);

    

    // xTaskCreate(pico_touch_screen_task, "touch_task", configMINIMAL_STACK_SIZE, NULL,
    //             WORKER_TASK_PRIORITY, NULL);

    xTaskCreate(
        network_manager_task,   // Task function
        "NetworkManager",       // Task name
        4096,                   // Stack size in words
        NULL,                   // Task parameters
        5,                      // Priority
        NULL                    // Task handle
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
