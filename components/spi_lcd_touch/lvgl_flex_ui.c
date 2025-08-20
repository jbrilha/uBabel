#include "esp_log.h"
#include "lvgl_ui.h"
#include <stdint.h>

static _lock_t *lvgl_lock = NULL;
static lv_obj_t *ui_col_0 = NULL;
static lv_obj_t *ui_col_1 = NULL;

lv_obj_t *lvgl_flex_layout_get_col(int32_t col_idx) {
    switch (col_idx) {
    case 0:
        return ui_col_0;
    case 1:
        return ui_col_1;
    }

    return NULL;
}

lv_obj_t *flex_col_create_left(lv_obj_t *container, int32_t w, int32_t h) {
    lv_obj_t *col = lv_obj_create(container);
    lv_obj_set_size(col, w, h);
    lv_obj_align(col, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    return col;
}
lv_obj_t *flex_col_create_right(lv_obj_t *container, int32_t w, int32_t h) {
    lv_obj_t *col = lv_obj_create(container);
    lv_obj_set_size(col, w, h);
    lv_obj_align(col, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    return col;
}

void lvgl_flex_layout_init(lv_display_t *disp, _lock_t *lock) {
    lvgl_lock = lock;

    if (lvgl_lock) {
        _lock_acquire(lvgl_lock);
        lv_obj_t *scr = lv_display_get_screen_active(disp);

        lv_area_t ar;
        lv_obj_get_coords(scr, &ar);
        int32_t screen_w = ar.x2 + 1;
        int32_t screen_h = ar.y2 + 1;

        ui_col_0 = flex_col_create_left(scr, 3 * screen_w / 4, screen_h);
        ui_col_1 = flex_col_create_right(scr, screen_w / 4, screen_h);

        _lock_release(lvgl_lock);
    }
}
