#include "app.h"

#include "platform.h"
#include "proto_iot_control.h"
#include <stdlib.h>
#include <string.h>

#include "event_dispatcher.h"

#include "comm_manager.h"
#include "common_events.h"
#include "event_dispatcher.h"
#include "network_events.h"
#include "proto_iot_control.h"
#include "proto_simple_overlay.h"

#if BUILD_ESP32
// TODO place somewhere else
#define EVENT_BUTTON_A_PRESSED 201
#define EVENT_BUTTON_B_PRESSED 202
#define EVENT_BUTTON_X_PRESSED 203
#define EVENT_BUTTON_Y_PRESSED 204
#elif BUILD_PICO
#include "pico_tasks.h"
#endif

#define REQUEST_PRINT 800
#define REQUEST_SHOW 801

#define APP_TASK_PRIORITY (tskIDLE_PRIORITY + 2UL)
#define APP_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE)

static const char *TAG = "APPLICATION";

static void scroll_node(iot_node_handle_t device) {
    static char device_id[16 * 2 + 5];
    memset(device_id, 0, 16 * 2 + 5);

    char *text = (char *)malloc(256);

    if (text != NULL) {
        memset(text, 0, 256);
        printf(
            "Going to print the node identifier to a textual representation\n");
        print_node_identifier(device, device_id);
        printf("Got the node identifier %s\n", device_id);
        sprintf(text, "Current device: %s", device_id);
        event_t *print_event =
            create_event(EVENT_TYPE_REQUEST, REQUEST_PRINT, text, 256);

        if (print_event == NULL || !event_dispatcher_post(print_event)) {
            if (print_event != NULL) {
                free_event(print_event);
            } else {
                free(text);
            }
        }
    }
}

static void show_device(iot_node_handle_t node_handle,
                        iot_device_handle_t device_handle,
                        device_t *device_info) {

    char *text = (char *)malloc(9);

    if (text != NULL) {
        memset(text, 0, 9);
        LOG_INFO(TAG, "Going to print a device with device_handle %d",
                 device_handle);
        if (device_handle < 0 || device_info == NULL) {
            switch (device_handle) {
            case NO_DEVICE:
                sprintf(text, "NO DEV");
                break;
            case INVALID_NODE:
            default:
                sprintf(text, "ERR NODE");
                break;
            }
        } else {
            LOG_INFO(TAG, "Going to print a device with device type %d",
                     device_info->device_type);
            free(text);
            text = strdup(device_info->device_name);
            if (text == NULL) {
                LOG_ERROR(TAG, "Failed to allocate memory for device name");
                return;
            }
        }

        event_t *print_event =
            create_event(EVENT_TYPE_REQUEST, REQUEST_PRINT, text, 8);
        if (print_event == NULL || !event_dispatcher_post(print_event)) {
            if (print_event != NULL) {
                free_event(print_event);
            } else {
                free(text);
            }
        }
    }
}

typedef enum { node, device, action, parameter } navigation_mode_t;

static QueueHandle_t application_queue;
static iot_node_handle_t node_handle;
static iot_device_handle_t device_handle;
static navigation_mode_t nav;

static const device_t *device_info; // From IoTControl protocol
static device_t *current_device = NULL;
static action_t *current_action = NULL;
static parameter_t *current_parameter = NULL;

static device_t *find_correct_device(uint8_t device_type) {
    device_t *current = (device_t *)device_info;
    while (current != NULL) {
        if (current->device_type == device_type) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static void show_error(const char *message) {
    if (message != NULL) {
        char *text = strdup(message);
        if (text != NULL) {
            event_t *print_event = create_event(
                EVENT_TYPE_REQUEST, REQUEST_PRINT, text, strlen(text));
            if (print_event == NULL || !event_dispatcher_post(print_event)) {
                if (print_event != NULL) {
                    free_event(print_event);
                } else {
                    free(text);
                }
            }
        }
    }
}

static void show_action(action_t *action_info) {
    char *text = NULL;

    if (action_info != NULL) {
        text = strdup(action_info->action_name);
    } else {
        text = strdup("No action selected");
    }

    if (text != NULL) {
        event_t *print_event =
            create_event(EVENT_TYPE_REQUEST, REQUEST_PRINT, text, strlen(text));
        if (print_event == NULL || !event_dispatcher_post(print_event)) {
            if (print_event != NULL) {
                free_event(print_event);
            } else {
                free(text);
            }
        }
    }
}

static void show_parameter(parameter_t *parameter) {
    char *text = NULL;

    if (parameter != NULL) {
        text = strdup(parameter->parameter_name);
    } else {
        text = strdup("Action does not have parameters");
    }

    if (text != NULL) {
        event_t *print_event =
            create_event(EVENT_TYPE_REQUEST, REQUEST_PRINT, text, strlen(text));
        if (print_event == NULL || !event_dispatcher_post(print_event)) {
            if (print_event != NULL) {
                free_event(print_event);
            } else {
                free(text);
            }
        }
    }
}

void application_task(void *pvParameters) {
    char output[256];
    event_t *event = NULL;

    while (true) {
        memset(output, 0, 256); // Clear text buffer
        if (xQueueReceive(application_queue, &event, portMAX_DELAY) == pdTRUE) {
            printf("Application main loop has received event: type=%d "
                   "subtype=%d\n",
                   event->type, event->subtype);

            if (event->type == EVENT_TYPE_NOTIFICATION) {

                if (event->subtype == EVENT_BUTTON_A_PRESSED) {
                    // go up a level or execute
                    switch (nav) {
                    case node:
                        nav = device;
                        device_handle = initialize_device_iterator(node_handle);
                        if (device_handle == INVALID_NODE) {
                            printf("There is something wrong with this node "
                                   "reference (might have failed): %d\n",
                                   device_handle);
                            show_error("Invalid node");
                            nav = node;
                            node_handle = NULL;
                        } else if (device_handle == NO_DEVICE) {
                            printf(
                                "MThere is no valid device in this node: %d\n",
                                device_handle);
                            show_error("No valid device");
                            nav = node;
                            node_handle = NULL;
                        } else {
                            uint16_t device_typology =
                                get_device_type(node_handle, device_handle);
                            current_device =
                                find_correct_device(device_typology);
                            printf("Main Action, device handle is: %d\n",
                                   device_handle);
                            show_device(node_handle, device_handle,
                                        current_device);
                        }
                        break;
                    case device:
                        if (current_device == NULL) {
                            nav = device;
                            uint16_t device_typology =
                                get_device_type(node_handle, device_handle);
                            current_device =
                                find_correct_device(device_typology);
                            show_error("No valid device");
                        } else {
                            nav = action;
                            current_action = current_device->actions;
                            show_action(current_action);
                        }
                        break;
                    case action:
                        if (current_device == NULL) {
                            nav = device;
                            uint16_t device_typology =
                                get_device_type(node_handle, device_handle);
                            current_device =
                                find_correct_device(device_typology);
                            show_error("No valid device");
                        } else if (current_action == NULL) {
                            show_error("No valid action");
                        } else {
                            nav = parameter;
                            current_parameter = current_action->parameters;
                            show_parameter(current_parameter);
                        }
                        break;
                    case parameter:
                        // This is to effectively execute the action over the
                        // device using the appropriate parameters
                        device_action(node_handle, device_handle,
                                      current_device, current_action,
                                      current_parameter);
                        break;
                    default:
                        LOG_ERROR(TAG, "Unknown navigation mode");
                        break;
                    }

                } else if (event->subtype == EVENT_BUTTON_B_PRESSED) {
                    // Previous element in the list
                    switch (nav) {
                    case node:
                        node_handle = previous_node(node_handle);
                        scroll_node(node_handle);
                        break;
                    case device:
                        device_handle =
                            previous_device(node_handle, device_handle);
                        uint16_t device_typology =
                            get_device_type(node_handle, device_handle);
                        current_device = find_correct_device(device_typology);
                        printf("Main Action, device handle is: %d\n",
                               device_handle);
                        show_device(node_handle, device_handle, current_device);
                        break;
                    case action:
                        if (current_action != NULL) {
                            current_action = current_action->prev;
                            show_action(current_action);
                        } else {
                            nav = device;
                            show_error("No action available");
                        }
                        break;
                    case parameter:
                        if (current_parameter != NULL) {
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

                } else if (event->subtype == EVENT_BUTTON_X_PRESSED) {
                    // Go down a level (or in node go to broadcast)
                    switch (nav) {
                    case node:
                        // nothing to be done
                    case device:
                        nav = node;
                        if (node_handle == NULL)
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

                } else if (event->subtype == EVENT_BUTTON_Y_PRESSED) {
                    // Next element on the list
                    switch (nav) {
                    case node:
                        node_handle = previous_node(node_handle);
                        scroll_node(node_handle);
                        break;
                    case device:
                        device_handle = next_device(node_handle, device_handle);
                        uint16_t device_typology =
                            get_device_type(node_handle, device_handle);
                        current_device = find_correct_device(device_typology);
                        printf("Main Action, device handle is: %d\n",
                               device_handle);
                        show_device(node_handle, device_handle, current_device);
                        break;
                    case action:
                        if (current_action != NULL) {
                            current_action = current_action->next;
                            show_action(current_action);
                        } else {
                            nav = device;
                            show_error("No action available");
                        }
                        break;
                    case parameter:
                        if (current_parameter != NULL) {
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
    application_queue = xQueueCreate(10, sizeof(event_t *));
    event_dispatcher_register(application_queue, EVENT_TYPE_NOTIFICATION,
                              EVENT_BUTTON_A_PRESSED);
    event_dispatcher_register(application_queue, EVENT_TYPE_NOTIFICATION,
                              EVENT_BUTTON_B_PRESSED);
    event_dispatcher_register(application_queue, EVENT_TYPE_NOTIFICATION,
                              EVENT_BUTTON_X_PRESSED);
    event_dispatcher_register(application_queue, EVENT_TYPE_NOTIFICATION,
                              EVENT_BUTTON_Y_PRESSED);

    nav = node;
    device_info = get_device_info_data();

    node_handle = initialize_node_iterator();

    xTaskCreate(application_task, "app_task", APP_TASK_STACK_SIZE, NULL,
                APP_TASK_PRIORITY, NULL);
}
