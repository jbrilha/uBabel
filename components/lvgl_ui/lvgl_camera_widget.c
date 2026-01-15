#include "lvgl_camera_widget.h"
#include "core/lv_obj_pos.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "misc/lv_color.h"

static const char* TAG = "CAMERA_WIDGET";

static SemaphoreHandle_t lvgl_lock = NULL;
static lv_obj_t *cam_image = NULL;
static lv_image_dsc_t img_dsc = {0};
static lv_obj_t *shutter_btn = NULL;
static lv_obj_t *shutter_lbl = NULL;

static void shutter_btn_cb(lv_event_t *e) {
    // TODO actually snap a picture
}

static lv_obj_t *shutter_lbl_create(lv_obj_t *container) {
    lv_obj_t *lbl = lv_label_create(container);
    lv_label_set_text_static(lbl, "TEST");
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
    return lbl;
}

static lv_obj_t *shutter_btn_create(lv_obj_t *container) {
    lv_obj_t *btn = lv_button_create(container);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text_static(lbl, LV_SYMBOL_VIDEO);
    lv_obj_align(btn, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_add_event_cb(btn, shutter_btn_cb, LV_EVENT_CLICKED, NULL);

    return btn;
}

static lv_obj_t *image_create(lv_obj_t *container) {
    int32_t h = lv_obj_get_height(container);
    int32_t w = lv_obj_get_width(container);

    lv_obj_t *img = lv_image_create(container);
    lv_obj_set_size(img, w, h);
    lv_obj_center(img);

    return img;
}

static void update_camera_feed(uint8_t *buf, size_t len) {
    int32_t w = lv_obj_get_width(lv_obj_get_parent(cam_image));
    int32_t h = lv_obj_get_height(lv_obj_get_parent(cam_image));
    size_t expected_len = w * h * 2; // RGB565 = 16 bits per pixel

    if (len != expected_len) {
        ESP_LOGE(TAG, "buf size does not match display size, expect UI corruption");
        // return; 
    }

    img_dsc.header.w = w;
    img_dsc.header.h = h;
    img_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    img_dsc.data_size = len;
    img_dsc.data = buf;

    lv_image_set_src(cam_image, &img_dsc);
}

void camera_widget_set_feed(uint8_t *buf, size_t len) {
    if (buf && len > 0 && cam_image && lvgl_lock) {
        xSemaphoreTake(lvgl_lock, portMAX_DELAY);
        update_camera_feed(buf, len);
        xSemaphoreGive(lvgl_lock);
    }
}

void camera_widget_init(lv_display_t *disp, SemaphoreHandle_t lock) {
    lv_obj_t *scr = lv_display_get_screen_active(disp);

    camera_widget_init_on_container(scr, lock);
}

void camera_widget_init_on_container(lv_obj_t *container, SemaphoreHandle_t lock) {
    lvgl_lock = lock;

    if (lvgl_lock) {
        xSemaphoreTake(lvgl_lock, portMAX_DELAY);
        if (!cam_image) {
            cam_image = image_create(container);
            shutter_btn = shutter_btn_create(container);
            shutter_lbl = shutter_lbl_create(container);
        }
        xSemaphoreGive(lvgl_lock);
    }
}
