#include "esp32c6_buttons.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "pcf8574.h"
#include <stdint.h>

#include "event_dispatcher.h"

void c6_buttons_task(void *arg) {
    uint8_t last_read = 0xFF;
    while (1) {
        uint8_t read = PCF8574_read_8();

        uint8_t changed = read ^ last_read;

        for (int i = 0; i < 8; i++) {
            if (changed & (1 << i)) {
                bool pressed = !(read & (1 << i)); // LOW = pressed
                if (pressed) {
                    event_t *event = NULL;
                    ESP_LOGI("PCF8574", "button P%d PRESSED", i);
                    switch (i) {
                    case 0:
                        event =
                            create_event(EVENT_TYPE_NOTIFICATION,
                                         EVENT_ESP32C6_DOWN_PRESSED, NULL, 0);
                        break;
                    // case 1:
                    //     break;
                    // case 2:
                    //     event =
                    //         create_event(EVENT_TYPE_NOTIFICATION,
                    //                      EVENT_ESP32C6_RIGHT_PRESSED, NULL, 0);
                    //     break;
                    case 3:
                        event = create_event(EVENT_TYPE_NOTIFICATION,
                                             EVENT_ESP32C6_B_PRESSED, NULL, 0);
                        break;
                    // case 4:
                    //     event = create_event(EVENT_TYPE_NOTIFICATION,
                    //                          EVENT_ESP32C6_A_PRESSED, NULL, 0);
                    //     break;
                    // case 5:
                    //     break;
                    case 6:
                        event = create_event(EVENT_TYPE_NOTIFICATION,
                                             EVENT_ESP32C6_UP_PRESSED, NULL, 0);
                        break;
                    // case 7:
                    //     event =
                    //         create_event(EVENT_TYPE_NOTIFICATION,
                    //                      EVENT_ESP32C6_LEFT_PRESSED, NULL, 0);
                    //     break;
                    }

                    if (event) {
                        event_dispatcher_post(event);
                    }
                }
            }
        }

        last_read = read;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static bool initialized = false;

void esp32c6_buttons_init() { initialized = PCF8574_init(); }

void run_esp32c6_buttons_task(void) {
    if (initialized)
        xTaskCreate(c6_buttons_task, "c6_buttons", 2048, NULL, 2, NULL);
    else
        ESP_LOGE("C6_buttons", "FAILED TO LAUNCH BUTTONS TASK");
}
