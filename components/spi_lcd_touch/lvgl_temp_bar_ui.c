#include "lvgl_ui.h"

#define BAR_MIN 0
#define BAR_MAX 40

static _lock_t *lvgl_lock = NULL;
static lv_obj_t *bar = NULL;

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

static lv_obj_t *create_bar(lv_obj_t *scr, lv_style_t *style) {
    if (bar) {
        return bar;
    }

    lv_obj_t *new_bar = lv_bar_create(scr);

    if (style) {
        lv_obj_add_style(new_bar, style, LV_PART_INDICATOR);
    }

    lv_obj_set_size(new_bar, 20, 200);
    lv_obj_center(new_bar);
    lv_bar_set_range(new_bar, BAR_MIN, BAR_MAX);

    return new_bar;
}

static void set_animation() {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, set_temp);
    lv_anim_set_duration(&a, 3000);
    lv_anim_set_reverse_duration(&a, 3000);
    lv_anim_set_var(&a, bar);
    lv_anim_set_values(&a, BAR_MIN, BAR_MAX);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

void lvgl_temp_bar_set(int32_t temp) {
    if (bar && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        lv_bar_set_value(bar, temp, LV_ANIM_OFF);
        _lock_release(lvgl_lock);
    }
}

void lvgl_temp_bar_animate_to(int32_t temp) {
    if (bar && lvgl_lock) {
        _lock_acquire(lvgl_lock);

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_exec_cb(&a, set_temp);
        lv_anim_set_duration(&a, 1500);
        lv_anim_set_var(&a, bar);
        lv_anim_set_values(&a, BAR_MIN, temp);
        lv_anim_set_repeat_count(&a, 1);
        lv_anim_start(&a);
        _lock_release(lvgl_lock);
    }
}

void lvgl_temp_bar_init(lv_display_t *disp, _lock_t *lock) {
    lvgl_lock = lock;
    lv_obj_t *scr = lv_display_get_screen_active(disp);

    static lv_style_t style;
    init_style(&style);

    bar = create_bar(scr, &style);

    set_animation();
}
