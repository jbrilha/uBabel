#include "lvgl_lora_sender_widget.h"
#include "esp_log.h"

#include "lora_types.h"
#include <stdio.h>

#define RSSI_BAR_MIN (-157)
#define RSSI_BAR_MAX (-30)

#define SNR_BAR_MIN (-100)
#define SNR_BAR_MAX (200)

// #define BAR_WIDTH 280
#define BAR_WIDTH 260
#define BAR_HEIGHT 15

#define ANIM_DURATION 2000

static _lock_t *lvgl_lock = NULL;
static lv_obj_t *sender_txt = NULL;
static lv_obj_t *recipient_txt = NULL;
static lv_obj_t *message_id_txt = NULL;
static lv_obj_t *message_txt = NULL;

static void btn_cb(lv_event_t *e) {
    // todo send request via event queue
}

static lv_obj_t *create_scrolling_text(lv_obj_t *container, bool underline,
                                       const char *lbl_txt,
                                       const char *plchldr_txt) {
    lv_obj_t *bar_row = lv_obj_create(container);
    lv_obj_remove_style_all(bar_row);
    lv_obj_set_size(bar_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bar_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(bar_row, 5, 0);

    lv_obj_t *label = lv_label_create(bar_row);
    lv_label_set_text_static(label, lbl_txt);

    lv_obj_t *txt = lv_label_create(bar_row);
    lv_obj_remove_style_all(txt);

    if (underline) {
        static lv_style_t underline_style;
        lv_style_init(&underline_style);
        lv_style_set_text_decor(&underline_style, LV_TEXT_DECOR_UNDERLINE);
        lv_obj_add_style(txt, &underline_style, 0);

        lv_label_set_long_mode(txt, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
        lv_obj_set_width(txt, 150);
    } else {

        lv_label_set_long_mode(txt, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_size(txt, 230, 120);
        lv_obj_set_scrollbar_mode(txt, LV_SCROLLBAR_MODE_AUTO);

        lv_obj_set_style_border_color(txt, lv_color_black(), 0);
        lv_obj_set_style_border_width(txt, 1, 0);
        lv_obj_set_style_border_side(txt, LV_BORDER_SIDE_FULL, 0);
    }

    lv_label_set_text(txt, plchldr_txt);

    return txt;
}

static lv_obj_t *create_text(lv_obj_t *container, bool underline,
                             const char *lbl_txt, const char *plchldr_txt) {
    lv_obj_t *bar_row = lv_obj_create(container);
    lv_obj_remove_style_all(bar_row);
    lv_obj_set_size(bar_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bar_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(bar_row, 5, 0);

    lv_obj_t *label = lv_label_create(bar_row);
    lv_label_set_text_static(label, lbl_txt);

    lv_obj_t *txt = lv_label_create(bar_row);
    lv_obj_remove_style_all(txt);

    lv_obj_set_width(txt, 100);
    lv_label_set_text(txt, plchldr_txt);

    if (underline) {
        static lv_style_t underline_style;
        lv_style_init(&underline_style);
        lv_style_set_text_decor(&underline_style, LV_TEXT_DECOR_UNDERLINE);
        lv_obj_add_style(txt, &underline_style, 0);
    }

    return txt;
}

static lv_obj_t *create_sender_txt(lv_obj_t *container) {
    return create_scrolling_text(container, true, "Sender:", "Me");
}

static lv_obj_t *create_recipient_txt(lv_obj_t *container) {
    return create_scrolling_text(container, true, "Recipient:", "Not me");
}

static lv_obj_t *create_message_txt(lv_obj_t *container) {
    return create_scrolling_text(container, false,
                                 "Message:", "No outgoing messages");
}

static lv_obj_t *create_message_id_txt(lv_obj_t *container) {
    return create_text(container, true, "Message ID:", "0x00");
}

void lora_sndr_widget_set_message_id_txt(const char *id) {
    if (message_id_txt && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        lv_label_set_text(message_id_txt, id);
        _lock_release(lvgl_lock);
    }
}

void lora_sndr_widget_set_message_txt(const char *txt) {
    if (message_txt && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        lv_label_set_text(message_txt, txt);
        _lock_release(lvgl_lock);
    }
}

void lora_sndr_widget_set_sender_txt(const char *sender) {
    if (sender_txt && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        lv_label_set_text(sender_txt, sender);
        _lock_release(lvgl_lock);
    }
}

void lora_sndr_widget_set_recipient_txt(const char *recipient) {
    if (recipient_txt && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        lv_label_set_text(recipient_txt, recipient);
        _lock_release(lvgl_lock);
    }
}

void lora_sndr_widget_send_transmission(event_t *e) {
    lora_info_t info = *(lora_info_t *)e->payload;

    char sender_str[8];
    snprintf(sender_str, sizeof(sender_str), "0x%02X", info.pkt->sender_id);
    lora_sndr_widget_set_sender_txt(sender_str);

    char recipient_str[8];
    snprintf(recipient_str, sizeof(recipient_str), "0x%02X",
             info.pkt->recipient_id);
    lora_sndr_widget_set_recipient_txt(recipient_str);

    char msg_id_str[8];
    snprintf(msg_id_str, sizeof(msg_id_str), "%d", info.pkt->message_id);
    lora_sndr_widget_set_message_id_txt(msg_id_str);

    char payload_str[info.pkt->payload_len + 1];
    memcpy(payload_str, info.pkt->payload, info.pkt->payload_len);
    payload_str[info.pkt->payload_len] = '\0';
    lora_sndr_widget_set_message_txt(payload_str);
}

void lora_sndr_widget_init(lv_display_t *disp, _lock_t *lock) {
    lv_obj_t *scr = lv_display_get_screen_active(disp);

    lora_sndr_widget_init_on_container(scr, lock);
}

void lora_sndr_widget_init_on_container(lv_obj_t *container, _lock_t *lock) {
    lvgl_lock = lock;

    if (lvgl_lock) {
        _lock_acquire(lvgl_lock);
        lv_obj_t *bars_container = lv_obj_create(container);
        lv_obj_remove_style_all(bars_container);
        lv_obj_set_size(bars_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_align(bars_container, LV_ALIGN_LEFT_MID, 10, 0);

        lv_obj_set_flex_flow(bars_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(bars_container, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_gap(bars_container, 5, 0);

        if (!sender_txt) {
            sender_txt = create_sender_txt(bars_container);
        }
        if (!recipient_txt) {
            recipient_txt = create_recipient_txt(bars_container);
        }
        if (!message_id_txt) {
            message_id_txt = create_message_id_txt(bars_container);
        }
        if (!message_txt) {
            message_txt = create_message_txt(bars_container);
        }
        _lock_release(lvgl_lock);
    }
}
