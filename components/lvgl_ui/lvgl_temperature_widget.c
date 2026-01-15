#include "lvgl_temperature_widget.h"
#include "freertos/FreeRTOS.h"

#define BAR_MIN 0
#define BAR_MAX 40

#define BAR_WIDTH 20
#define BAR_HEIGHT 150

#define ANIM_DURATION 2000

static SemaphoreHandle_t lvgl_lock = NULL;
static lv_obj_t *ui_bar = NULL;
static lv_obj_t *ui_btn = NULL;

static void set_temp(void *bar, int32_t temp) {
    lv_bar_set_value((lv_obj_t *)bar, temp, LV_ANIM_ON);
}

// gradient from blue to red, looks neat
static void init_style(lv_style_t *style) {
    lv_style_init(style);
    lv_style_set_bg_opa(style, LV_OPA_COVER);
    lv_style_set_bg_color(style, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_bg_grad_color(style, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_grad_dir(style, LV_GRAD_DIR_VER);
}

static void btn_cb(lv_event_t *e) {
    // todo send request via event queue
}

static void event_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target_obj(e);

    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.font = LV_FONT_DEFAULT;

    char buf[8];
    int val = lv_bar_get_value(obj);
    lv_snprintf(buf, sizeof(buf), "%d", val);

    lv_point_t txt_size;
    lv_text_get_size(&txt_size, buf, label_dsc.font, label_dsc.letter_space,
                     label_dsc.line_space, LV_COORD_MAX, label_dsc.flag);

    lv_area_t bar_area;
    lv_obj_get_coords(obj, &bar_area);
    lv_coord_t bar_height = lv_area_get_height(&bar_area);

    lv_area_t indic_area;
    lv_coord_t indic_height = bar_height * val / BAR_MAX;

    indic_area.x1 = bar_area.x1;
    indic_area.x2 = bar_area.x2;
    indic_area.y2 = bar_area.y2;
    indic_area.y1 = bar_area.y2 - indic_height;

    lv_area_t txt_area;
    txt_area.x1 = 0;
    txt_area.x2 = txt_size.x - 1;
    txt_area.y1 = 0;
    txt_area.y2 = txt_size.y - 1;

    int32_t h = lv_area_get_height(&indic_area);
    int32_t thresh = txt_size.y * 2;

    if (h < thresh) {
        lv_area_align(&indic_area, &txt_area, LV_ALIGN_OUT_TOP_MID, 0, 0);
        label_dsc.color = lv_color_black();
    } else if (h <= thresh * 2 - 5) {
        lv_area_t fixed_area = bar_area;
        fixed_area.y1 = bar_area.y2 - thresh;
        fixed_area.y2 = fixed_area.y1;

        lv_area_align(&fixed_area, &txt_area, LV_ALIGN_OUT_TOP_MID, 0, 0);
        if (h < thresh + txt_size.y) {
            label_dsc.color = lv_color_black();
        } else {
            label_dsc.color = lv_color_white();
        }
    } else {
        lv_area_align(&indic_area, &txt_area, LV_ALIGN_TOP_MID, 0, 10);
        label_dsc.color = lv_color_white();
    }

    label_dsc.text = buf;
    label_dsc.text_local = true;
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_draw_label(layer, &label_dsc, &txt_area);
}

static void set_animation(lv_obj_t *bar) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, set_temp);
    lv_anim_set_duration(&a, ANIM_DURATION);
    lv_anim_set_reverse_duration(&a, ANIM_DURATION);
    lv_anim_set_var(&a, bar);
    lv_anim_set_values(&a, BAR_MIN, BAR_MAX);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static lv_obj_t *temperature_fetch_btn_create(lv_obj_t *container) {
    lv_obj_t *btn = lv_button_create(container);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text_static(lbl, LV_SYMBOL_REFRESH);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, NULL);

    return btn;
}

static lv_obj_t *temperature_bar_create(lv_obj_t *container, bool use_style,
                                        bool animate) {
    lv_obj_t *new_bar = lv_bar_create(container);

    if (use_style) {
        static lv_style_t style;
        init_style(&style);
        lv_obj_add_style(new_bar, &style, LV_PART_INDICATOR);
    }

    lv_obj_t *lbl = lv_label_create(container);
    lv_label_set_text_static(lbl, "°C");

    lv_obj_set_size(new_bar, BAR_WIDTH, BAR_HEIGHT);
    lv_obj_center(new_bar);
    lv_bar_set_range(new_bar, BAR_MIN, BAR_MAX);

    lv_obj_align_to(lbl, new_bar, LV_ALIGN_OUT_TOP_MID, 0, 0);

    lv_obj_add_event_cb(new_bar, event_cb, LV_EVENT_DRAW_MAIN_END, NULL);

    if (animate)
        set_animation(new_bar);

    return new_bar;
}

void temperature_bar_set_val(int32_t temp) {
    if (ui_bar && lvgl_lock) {
        xSemaphoreTake(lvgl_lock, portMAX_DELAY);
        lv_bar_set_value(ui_bar, temp, LV_ANIM_OFF);
        xSemaphoreGive(lvgl_lock);
    }
}

void temperature_bar_animate_to_val(int32_t temp) {
    if (ui_bar && lvgl_lock) {
        xSemaphoreTake(lvgl_lock, portMAX_DELAY);

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_exec_cb(&a, set_temp);
        lv_anim_set_duration(&a, 1500);
        lv_anim_set_var(&a, ui_bar);
        lv_anim_set_values(&a, BAR_MIN, temp);
        lv_anim_set_repeat_count(&a, 1);
        lv_anim_start(&a);
        xSemaphoreGive(lvgl_lock);
    }
}

void temperature_widget_init(lv_display_t *disp, SemaphoreHandle_t lock,
                             bool use_style, bool animate) {
    lv_obj_t *scr = lv_display_get_screen_active(disp);

    temperature_widget_init_on_container(scr, lock, use_style, animate);
}

void temperature_widget_init_on_container(lv_obj_t *container,
                                          SemaphoreHandle_t lock,
                                          bool use_style, bool animate) {
    lvgl_lock = lock;

    if (lvgl_lock) {
        xSemaphoreTake(lvgl_lock, portMAX_DELAY);
        if (!ui_bar) {
            ui_bar = temperature_bar_create(container, use_style, animate);
            ui_btn = temperature_fetch_btn_create(container);
        }
        xSemaphoreGive(lvgl_lock);
    }
}

// void temperature_flex_bar_init(lv_display_t *disp, SemaphoreHandle_t lock) {
//     lv_obj_t *scr = lv_display_get_screen_active(disp);
//
//     lv_area_t ar;
//     lv_obj_get_coords(scr, &ar);
//     int32_t screen_w = ar.x2;
//     int32_t screen_h = ar.y2;
//
//     lv_obj_t *cont_col_1 = lv_obj_create(scr);
//     lv_obj_set_size(cont_col_1, 3 * screen_w / 4, screen_h);
//     lv_obj_align(cont_col_1, LV_ALIGN_LEFT_MID, 0, 0);
//     lv_obj_set_flex_flow(cont_col_1, LV_FLEX_FLOW_COLUMN);
//     lv_obj_set_flex_align(cont_col_1, LV_FLEX_ALIGN_CENTER,
//                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
//
//     lv_obj_t *cont_col_2 = lv_obj_create(scr);
//     lv_obj_set_size(cont_col_2, screen_w / 4, screen_h);
//     lv_obj_align(cont_col_2, LV_ALIGN_RIGHT_MID, 0, 0);
//     lv_obj_set_flex_flow(cont_col_2, LV_FLEX_FLOW_COLUMN);
//     lv_obj_set_flex_align(cont_col_2, LV_FLEX_ALIGN_CENTER,
//                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
//
//     static lv_style_t style;
//     init_style(&style);
//
//     lv_obj_t *b1 = create_bar(cont_col_1, &style);
//     lv_obj_t *b2 = create_bar(cont_col_2, &style);
//
//     set_animation(b1);
//     set_animation(b2);
// }
//
//
// for (i = 0; i < 10; i++) {
//     lv_obj_t *obj;
//     lv_obj_t *label;
//
//     /*Add items to the row*/
//     obj = lv_button_create(cont_row);
//     lv_obj_set_size(obj, 100, LV_PCT(100));
//
//     label = lv_label_create(obj);
//     lv_label_set_text_fmt(label, "Item: %" LV_PRIu32 "", i);
//     lv_obj_center(label);
//
//     /*Add items to the column*/
//     obj = lv_button_create(cont_col_1);
//     lv_obj_set_size(obj, LV_PCT(100), LV_SIZE_CONTENT);
//
//     label = lv_label_create(obj);
//     lv_label_set_text_fmt(label, "Item: %" LV_PRIu32, i);
//     lv_obj_center(label);
// }
