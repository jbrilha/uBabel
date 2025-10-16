#include "camera.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "jpeg_decoder.h"
#include "lvgl_camera_widget.h"
#include "sensor.h"
#include <errno.h>

// I did not test this code with any other modules, but it's ripped from:
// https://github.com/espressif/esp32-camera/
// so should work
#define CAMERA_TYPE "OV5640"

#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1 // software reset instead
#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF 7
#define CAM_PIN_PCLK 13
#define CAM_PIN_XCLK 15
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5
#define CAM_PIN_D0 11
#define CAM_PIN_D1 9
#define CAM_PIN_D2 8
#define CAM_PIN_D3 10
#define CAM_PIN_D4 12
#define CAM_PIN_D5 18
#define CAM_PIN_D6 17
#define CAM_PIN_D7 16

static const char *TAG = "CAMERA";

esp_err_t camera_init(void) {
    static camera_config_t camera_config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,

        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,

        // XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_RGB565, // YUV422,GRAYSCALE,RGB565,JPEG
        .frame_size =
            // For ESP32, do not use sizes above
            //  QVGA when not JPEG. The performance of the
            //  ESP32-S series has improved a lot, but JPEG mode
            //  always gives better frame rates.
        FRAMESIZE_QVGA, // 320x240
        // FRAMESIZE_QHD,   // 2560x1440
        // FRAMESIZE_WQXGA, // 2560x1600
        // FRAMESIZE_P_FHD, // 1080x1920
        // FRAMESIZE_QSXGA, // 2560x1920
        // FRAMESIZE_5MP,   // 2592x1944

        .jpeg_quality = 1, // 0-63, for OV series camera sensors, lower number
                           // means higher quality
        .fb_count = 1, // When jpeg mode is used, if fb_count more than one, the
                       // driver will work in continuous mode.
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

void camera_task(void *params) {
    sensor_t *s = esp_camera_sensor_get();
    // this tells the camera to reverse the RGB order otherwise it looks fried
    // on the LVGL widgets — which is pretty cool tbh
    s->set_reg(s, 0x4300, 0xFF, 0x6F);
    while (1) {
        camera_fb_t *pic = esp_camera_fb_get();

        camera_widget_set_feed(pic->buf, pic->len);

        esp_camera_fb_return(pic);

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void run_camera_task(void) {
    if (camera_init() == ESP_OK) {
        xTaskCreate(camera_task, "camera_task", 4096, NULL, 5, NULL);
    } else {

        ESP_LOGE(TAG, "failed to init %s camera", CAMERA_TYPE);
    }
}
