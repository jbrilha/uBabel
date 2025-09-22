#include "ui_event_monitor.h"
#include "ui_manager.h"

#include "comm_manager.h"
#include "common_events.h"
#include "event_dispatcher.h"
#include "lora_events.h"
#include "lvgl_tardis_widget.h"
#include "lvgl_ui.h"
#include "network_events.h"

#ifdef M5STACK_CORE_BASIC
#include "m5_buttons.h"
#endif
#include "platform.h"

#define UI_MONITOR_TASK_STACK_SIZE (4 * 1024)
#define UI_MONITOR_TASK_PRIORITY 2

// TODO these are initialized in the application files
#define REQUEST_PRINT 800
#define REQUEST_SHOW 801

#define Q_LEN 10

static const char *TAG = "UI_EVENT_MONITOR";

QueueHandle_t ui_event_queue;

static void handle_ui_notif(event_t *e);
static void handle_ui_request(event_t *e);

static void ui_event_test_task(void *pvParameters) {

    vTaskDelay(pdMS_TO_TICKS(2000));

    int i = 0;
    while (true) {
        int32_t *temp = (int32_t *)malloc(sizeof(int32_t));
        if (temp) {
            *temp = i;

            event_t *event =
                create_event(EVENT_TYPE_NOTIFICATION, UI_EVENT_REC_TEMP, temp,
                             sizeof(int32_t));
            if (event) {
                event_dispatcher_post(event);
            }
        }

        char *msg = malloc(64);
        if (msg) {
            snprintf(msg, 64, "Hello %d", i);

            event_t *event =
                create_event(EVENT_TYPE_NOTIFICATION, UI_EVENT_REC_MSG, msg,
                             strlen(msg) + 1);
            if (event) {
                event_dispatcher_post(event);
            }
        }

        i++;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    vTaskDelete(NULL);
}

static void ui_event_monitor_task(void *pvParameters) {
    event_t *event = NULL;

    while (true) {
        if (xQueueReceive(ui_event_queue, &event, pdMS_TO_TICKS(10)) ==
            pdTRUE) {

            switch (event->type) {
            case EVENT_TYPE_NOTIFICATION:
                handle_ui_notif(event);
                break;
            case EVENT_TYPE_REQUEST:
                handle_ui_request(event);
                break;
            default:
                break;
            }
        }
    }

    vTaskDelete(NULL);
}

void ui_event_monitor_init(void) {
    ui_event_queue = xQueueCreate(Q_LEN, sizeof(event_t *));

#ifdef M5STACK_CORE_BASIC
    event_dispatcher_register(ui_event_queue, EVENT_TYPE_NOTIFICATION,
                              EVENT_M5_BUTTON_A_PRESSED);
    event_dispatcher_register(ui_event_queue, EVENT_TYPE_NOTIFICATION,
                              EVENT_M5_BUTTON_B_PRESSED);
    event_dispatcher_register(ui_event_queue, EVENT_TYPE_NOTIFICATION,
                              EVENT_M5_BUTTON_C_PRESSED);
#endif

    event_dispatcher_register(ui_event_queue, EVENT_TYPE_NOTIFICATION,
                              EVENT_SUBTYPE_NETWORK_UP);
    event_dispatcher_register(ui_event_queue, EVENT_TYPE_NOTIFICATION,
                              EVENT_SUBTYPE_NETWORK_DOWN);
    event_dispatcher_register(ui_event_queue, EVENT_TYPE_NOTIFICATION,
                              NOTIFICATION_NEIGHBOR_UP);
    event_dispatcher_register(ui_event_queue, EVENT_TYPE_NOTIFICATION,
                              NOTIFICATION_NEIGHBOR_DOWN);

    event_dispatcher_register(ui_event_queue, EVENT_TYPE_REQUEST,
                              REQUEST_PRINT);
    event_dispatcher_register(ui_event_queue, EVENT_TYPE_REQUEST, REQUEST_SHOW);
    event_dispatcher_register(ui_event_queue, EVENT_TYPE_REQUEST,
                              REQUEST_REFRESH_MENU);

    xTaskCreate(ui_event_monitor_task, "UI_EVENT_MONITOR_TASK",
                UI_MONITOR_TASK_STACK_SIZE, NULL, UI_MONITOR_TASK_PRIORITY,
                NULL);

    // xTaskCreate(ui_event_test_task, "UI_EVENT_TEST_TASK",
    //             UI_MONITOR_TASK_STACK_SIZE, NULL, UI_MONITOR_TASK_PRIORITY,
    //             NULL);
}

static void handle_ui_notif(event_t *e) {
    switch (e->subtype) {
    case UI_EVENT_REC_MSG: {
        const char *txt = (const char *)e->payload;
        messenger_widget_set_txt(txt);

    } break;
    case UI_EVENT_REC_TEMP: {
        int32_t temp = *(int32_t *)e->payload;
        temperature_bar_animate_to_val(temp);

    } break;
    case UI_EVENT_REC_LORA: {
        lora_rec_widget_set_info_from_event(e);
        break;
    }
    case UI_EVENT_SND_LORA: {
        lora_sndr_widget_send_transmission(e);
        break;
    }

    case EVENT_SUBTYPE_NETWORK_UP: {
        char text[256];
        printf("Network: %s\n", ((network_event_t *)e->payload)->ssid);
        printf("IP: %s\n", ((network_event_t *)e->payload)->ip);
        sprintf(text, "Network UP: %s %s",
                ((network_event_t *)e->payload)->ssid,
                ((network_event_t *)e->payload)->ip);
        puts(text);
        tardis_widget_set_network_up_info((network_event_t *)e->payload);
        break;
    }
    case EVENT_SUBTYPE_NETWORK_DOWN: {
        tardis_widget_set_network_down();
        break;
    }
    case NOTIFICATION_NEIGHBOR_UP: {
        char text[256];
        sprintf(text, "Neighbor up: %s", uuid_to_string((uint8_t *)e->payload));
        tardis_widget_set_notif_txt(text);
        break;
    }
    case NOTIFICATION_NEIGHBOR_DOWN: {
        char text[256];
        sprintf(text, "Neighbor down: %s",
                uuid_to_string((uint8_t *)e->payload));
        puts(text);
        tardis_widget_set_notif_txt(text);
        break;
    }
#ifdef M5STACK_CORE_BASIC
    case EVENT_M5_BUTTON_A_PRESSED: {
        tardis_widget_menu_prev();
        break;
    }
    case EVENT_M5_BUTTON_B_PRESSED: {
        tardis_widget_menu_select();
        break;
    }
    case EVENT_M5_BUTTON_C_PRESSED: {
        tardis_widget_menu_next();
        break;
    }
#endif
    default:
        break;
    }

    free_event(e);
}

static void handle_ui_request(event_t *e) {
    switch (e->subtype) {
    case REQUEST_PRINT:
    case REQUEST_SHOW: {
        char text[256];
        memcpy(text, e->payload, strlen((char *)e->payload));
        puts(text);
        // tardis_widget_set_print_txt(text);
    }
    case REQUEST_REFRESH_MENU: {
        tardis_widget_populate_menu();
        break;
    }
    default:
        break;
    }
}
