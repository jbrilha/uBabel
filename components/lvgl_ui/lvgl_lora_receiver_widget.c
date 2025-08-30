#include "lvgl_lora_receiver_widget.h"
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
static lv_obj_t *rssi_bar = NULL;
static lv_obj_t *snr_bar = NULL;
static lv_obj_t *sender_txt = NULL;
static lv_obj_t *recipient_txt = NULL;
static lv_obj_t *message_id_txt = NULL;
static lv_obj_t *freq_err_txt = NULL;
static lv_obj_t *message_txt = NULL;

static void set_val(void *bar, int32_t val) {
    lv_bar_set_value((lv_obj_t *)bar, val, LV_ANIM_ON);
}

static void init_style(lv_style_t *style, bool rssi) {
    lv_style_init(style);
    lv_style_set_bg_opa(style, LV_OPA_COVER);

    if (rssi) {
        lv_style_set_bg_color(style, lv_palette_main(LV_PALETTE_RED));
        lv_style_set_bg_grad_color(style, lv_palette_main(LV_PALETTE_GREEN));
    } else {
        lv_style_set_bg_color(style, lv_palette_main(LV_PALETTE_PURPLE));
        lv_style_set_bg_grad_color(style, lv_palette_main(LV_PALETTE_CYAN));
    }
    lv_style_set_bg_grad_dir(style, LV_GRAD_DIR_HOR);
}

static void lora_rec_widget_animate_to_val_unsafe(lv_obj_t *obj, int32_t val) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, set_val);
    lv_anim_set_duration(&a, 500);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, RSSI_BAR_MIN, val);
    lv_anim_set_repeat_count(&a, 1);
    lv_anim_start(&a);
}

static void btn_cb(lv_event_t *e) {
    // todo send request via event queue
}

static void rssi_event_cb(lv_event_t *e) {
    const char *fmt = (const char *)lv_event_get_user_data(e);
    lv_obj_t *obj = lv_event_get_target_obj(e);

    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.font = LV_FONT_DEFAULT;

    char buf[32];
    int val = lv_bar_get_value(obj);
    lv_snprintf(buf, sizeof(buf), fmt, val);

    lv_point_t txt_size;
    lv_text_get_size(&txt_size, buf, label_dsc.font, label_dsc.letter_space,
                     label_dsc.line_space, LV_COORD_MAX, label_dsc.flag);

    lv_area_t bar_area;
    lv_obj_get_coords(obj, &bar_area);

    lv_area_t txt_area;
    txt_area.x1 = 0;
    txt_area.x2 = txt_size.x - 1;
    txt_area.y1 = 0;
    txt_area.y2 = txt_size.y - 1;

    lv_area_align(&bar_area, &txt_area, LV_ALIGN_LEFT_MID, 10, 0);
    label_dsc.color = lv_color_black();

    label_dsc.text = buf;
    label_dsc.text_local = true;
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_draw_label(layer, &label_dsc, &txt_area);
}

static void snr_event_cb(lv_event_t *e) {
    const char *fmt = (const char *)lv_event_get_user_data(e);
    lv_obj_t *obj = lv_event_get_target_obj(e);

    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.font = LV_FONT_DEFAULT;

    char buf[32];
    int val = lv_bar_get_value(obj);
    float snr_display = val / 10.0f;
    sprintf(buf, fmt, snr_display);

    lv_point_t txt_size;
    lv_text_get_size(&txt_size, buf, label_dsc.font, label_dsc.letter_space,
                     label_dsc.line_space, LV_COORD_MAX, label_dsc.flag);

    lv_area_t bar_area;
    lv_obj_get_coords(obj, &bar_area);

    lv_area_t txt_area;
    txt_area.x1 = 0;
    txt_area.x2 = txt_size.x - 1;
    txt_area.y1 = 0;
    txt_area.y2 = txt_size.y - 1;

    lv_area_align(&bar_area, &txt_area, LV_ALIGN_LEFT_MID, 10, 0);
    label_dsc.color = lv_color_black();

    label_dsc.text = buf;
    label_dsc.text_local = true;
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_draw_label(layer, &label_dsc, &txt_area);
}

static lv_obj_t *create_bar(lv_obj_t *container, const char *lbl_txt, char *fmt,
                            bool rssi, int32_t min, int32_t max) {
    static lv_style_t rssi_style;
    static lv_style_t snr_style;
    static bool styles_initialized = false;

    if (!styles_initialized) {
        init_style(&rssi_style, true);
        init_style(&snr_style, false);
        styles_initialized = true;
    }

    lv_obj_t *bar_row = lv_obj_create(container);
    lv_obj_remove_style_all(bar_row);
    lv_obj_set_size(bar_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bar_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(bar_row, 5, 0);

    lv_obj_t *label = lv_label_create(bar_row);
    lv_label_set_text_static(label, lbl_txt);

    lv_obj_t *new_bar = lv_bar_create(bar_row);
    lv_obj_remove_style_all(new_bar);
    lv_obj_add_style(new_bar, rssi ? &rssi_style : &snr_style,
                     LV_PART_INDICATOR);
    lv_obj_set_size(new_bar, BAR_WIDTH, BAR_HEIGHT);
    lv_bar_set_range(new_bar, min, max);

    if (rssi) {
        lv_obj_add_event_cb(new_bar, rssi_event_cb, LV_EVENT_DRAW_MAIN_END,
                            fmt);
    } else {
        lv_obj_add_event_cb(new_bar, snr_event_cb, LV_EVENT_DRAW_MAIN_END, fmt);
    }

    return new_bar;
}

static lv_obj_t *create_snr_bar(lv_obj_t *container) {
    return create_bar(container, "SNR:", "%.f (dB)", false, SNR_BAR_MIN,
                      SNR_BAR_MAX);
}

static lv_obj_t *create_rssi_bar(lv_obj_t *container) {
    return create_bar(container, "RSSI:", "%d (dBm)", true, RSSI_BAR_MIN,
                      RSSI_BAR_MAX);
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
        lv_obj_set_size(txt, 230, 100);
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
    return create_scrolling_text(container, true, "Sender:", "Nobody at all");
}

static lv_obj_t *create_recipient_txt(lv_obj_t *container) {
    return create_scrolling_text(container, true, "Recipient:", "Me");
}

static lv_obj_t *create_message_txt(lv_obj_t *container) {
    return create_scrolling_text(container, false,
                                 "Message:", "Nothing to show");
}

static lv_obj_t *create_message_id_txt(lv_obj_t *container) {
    return create_text(container, true, "Message ID:", "0x00");
}

static lv_obj_t *create_freq_err_txt(lv_obj_t *container) {
    return create_text(container, true, "Freq Err:", "312Hz");
}

void lora_rec_widget_set_rssi(int32_t rssi) {
    if (rssi_bar && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        lv_bar_set_value(rssi_bar, rssi, LV_ANIM_OFF);
        _lock_release(lvgl_lock);
    }
}

void lora_rec_widget_animate_to_rssi(int32_t rssi) {
    if (rssi_bar && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        lora_rec_widget_animate_to_val_unsafe(rssi_bar, rssi);
        _lock_release(lvgl_lock);
    }
}

void lora_rec_widget_set_snr(float snr) {
    if (snr_bar && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        int32_t snr_scaled = (int32_t)(snr * 10.0f);
        lv_bar_set_value(snr_bar, snr_scaled, LV_ANIM_OFF);
        _lock_release(lvgl_lock);
    }
}

void lora_rec_widget_animate_to_snr(float snr) {
    if (snr_bar && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        int32_t snr_scaled = (int32_t)(snr * 10.0f);
        lora_rec_widget_animate_to_val_unsafe(snr_bar, snr_scaled);
        _lock_release(lvgl_lock);
    }
}

void lora_rec_widget_set_freq_err(int32_t err) {
    if (freq_err_txt && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        char str[16];
        snprintf(str, sizeof(str), "%ld (Hz)", err);
        lv_label_set_text(freq_err_txt, str);
        _lock_release(lvgl_lock);
    }
}

void lora_rec_widget_set_message_id_txt(const char *id) {
    if (message_id_txt && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        lv_label_set_text(message_id_txt, id);
        _lock_release(lvgl_lock);
    }
}

void lora_rec_widget_set_message_txt(const char *txt) {
    if (message_txt && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        lv_label_set_text(message_txt, txt);
        _lock_release(lvgl_lock);
    }
}

void lora_rec_widget_set_sender_txt(const char *sender) {
    if (sender_txt && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        lv_label_set_text(sender_txt, sender);
        _lock_release(lvgl_lock);
    }
}

void lora_rec_widget_set_recipient_txt(const char *recipient) {
    if (recipient_txt && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        lv_label_set_text(recipient_txt, recipient);
        _lock_release(lvgl_lock);
    }
}

void lora_rec_widget_set_info_from_event(event_t *e) {
    lora_info_t info = *(lora_info_t *)e->payload;

    char sender_str[8];
    snprintf(sender_str, sizeof(sender_str), "0x%02X", info.pkt->sender_id);
    lora_rec_widget_set_sender_txt(sender_str);

    char recipient_str[8];
    snprintf(recipient_str, sizeof(recipient_str), "0x%02X",
             info.pkt->recipient_id);
    lora_rec_widget_set_recipient_txt(recipient_str);

    char msg_id_str[8];
    snprintf(msg_id_str, sizeof(msg_id_str), "%d", info.pkt->message_id);
    lora_rec_widget_set_message_id_txt(msg_id_str);

    lora_rec_widget_animate_to_rssi(info.rssi);
    lora_rec_widget_animate_to_snr(info.snr);
    lora_rec_widget_set_freq_err(info.freq_err);

    char payload_str[info.pkt->payload_len + 1];
    memcpy(payload_str, info.pkt->payload, info.pkt->payload_len);
    payload_str[info.pkt->payload_len] = '\0';
    lora_rec_widget_set_message_txt(payload_str);
}

void lora_rec_widget_init(lv_display_t *disp, _lock_t *lock) {
    lv_obj_t *scr = lv_display_get_screen_active(disp);

    lora_rec_widget_init_on_container(scr, lock);
}

void lora_rec_widget_init_on_container(lv_obj_t *container, _lock_t *lock) {
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
        if (!freq_err_txt) {
            freq_err_txt = create_freq_err_txt(bars_container);
        }
        if (!snr_bar) {
            snr_bar = create_snr_bar(bars_container);
        }
        if (!rssi_bar) {
            rssi_bar = create_rssi_bar(bars_container);
        }
        if (!message_txt) {
            message_txt = create_message_txt(bars_container);
        }
        _lock_release(lvgl_lock);
    }
}
