#include "lvgl_messenger_widget.h"
#include "misc/lv_style.h"

static _lock_t *lvgl_lock = NULL;
static lv_obj_t *ui_txt = NULL;
static lv_obj_t *ui_btn = NULL;

static void btn_cb(lv_event_t *e) {
    // todo send message via event queue
}

static lv_obj_t *send_msg_btn_create(lv_obj_t *container) {
    lv_obj_t *btn = lv_button_create(container);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text_static(lbl, "Send hello " LV_SYMBOL_ENVELOPE);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, NULL);

    return btn;
}

static lv_obj_t *scrolling_text_create(lv_obj_t *container) {
    lv_obj_t *txt = lv_label_create(container);

    lv_label_set_long_mode(txt, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_width(txt, lv_pct(90));
    lv_label_set_text(txt, "No new messages :(");
    lv_obj_align(txt, LV_ALIGN_BOTTOM_MID, 0, 40);

    lv_obj_set_style_border_color(txt, lv_color_hex(0x8080ff), 0);
    lv_obj_set_style_border_width(txt, 2, 0);
    lv_obj_set_style_border_side(txt, LV_BORDER_SIDE_FULL, 0);

    return txt;
}

static lv_obj_t *separator_create(lv_obj_t *container) {
    lv_obj_t *sep = lv_obj_create(container);

    lv_obj_set_size(sep, lv_pct(80), 2);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x808080), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_align(sep, LV_ALIGN_CENTER, 0, 0);

    return sep;
}

void messenger_widget_set_txt(const char *txt) {
    if (ui_txt && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        lv_label_set_text(ui_txt, txt);
        // this is just to separate it a bit
        lv_label_ins_text(ui_txt, LV_LABEL_POS_LAST, "    | ");
        _lock_release(lvgl_lock);
    }
}

void messenger_widget_init(lv_display_t *disp, _lock_t *lock) {
    lv_obj_t *scr = lv_display_get_screen_active(disp);

    messenger_widget_init_on_container(scr, lock);
}

void messenger_widget_init_on_container(lv_obj_t *container, _lock_t *lock) {
    lvgl_lock = lock;

    if (lvgl_lock) {
        _lock_acquire(lvgl_lock);
        if (!ui_txt) {
            ui_txt = scrolling_text_create(container);
            separator_create(container);
            ui_btn = send_msg_btn_create(container);
        }
        _lock_release(lvgl_lock);
    }
}
