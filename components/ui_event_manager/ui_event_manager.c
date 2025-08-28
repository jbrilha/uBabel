#include "ui_event_manager.h"

#include "event.h"
#include "event_dispatcher.h"
#include "freertos/projdefs.h"
#include "lvgl_ui.h"

#define UI_MANAGER_TASK_STACK_SIZE (4 * 1024)
#define UI_MANAGER_TASK_PRIORITY 2


#define Q_LEN 10

static const char *TAG = "UI_EVENT_MANAGER";

QueueHandle_t ui_event_queue;

static void handle_ui_notif(event_t *e);
static void handle_ui_request(event_t *e);

void ui_event_test_task(void *pvParameters) {

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

void ui_event_manager_init(void) {

    ui_event_queue = xQueueCreate(Q_LEN, sizeof(event_t *));
    event_dispatcher_register(ui_event_queue, EVENT_TYPE_NOTIFICATION,
                              UI_EVENT_REC_TEMP);
    event_dispatcher_register(ui_event_queue, EVENT_TYPE_NOTIFICATION,
                              UI_EVENT_REC_MSG);

    event_dispatcher_register(ui_event_queue, EVENT_TYPE_NOTIFICATION,
                              UI_EVENT_REC_LORA);

    xTaskCreate(ui_event_manager_task, "UI_EVENT_MANAGER_TASK",
                UI_MANAGER_TASK_STACK_SIZE, NULL, UI_MANAGER_TASK_PRIORITY,
                NULL);

    // xTaskCreate(ui_event_test_task, "UI_EVENT_TEST_TASK",
    //             UI_MANAGER_TASK_STACK_SIZE, NULL, UI_MANAGER_TASK_PRIORITY,
    //             NULL);
}

void ui_event_manager_task(void *pvParameters) {
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
        lora_payload_t lp = *(lora_payload_t *)e->payload;
        lora_widget_animate_to_rssi(lp.rssi);
        lora_widget_animate_to_snr(lp.snr);
        lora_widget_set_freq_err(lp.freq_err);
        lora_widget_set_message_txt((const char *)lp.payload);
    }
    default:
        break;
    }

    free_event(e);
}

static void handle_ui_request(event_t *e) {
    switch (e->subtype) {
    default:
        break;
    }
}
