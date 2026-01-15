#include "lvgl_flex_layout.h"

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

lv_obj_t *flex_col_create_left(lv_obj_t *container) {
    lv_obj_t *col = lv_obj_create(container);

    lv_obj_set_size(col, lv_pct(75), lv_pct(95));
    lv_obj_align(col, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    return col;
}

lv_obj_t *flex_col_create_right(lv_obj_t *container) {
    lv_obj_t *col = lv_obj_create(container);

    lv_obj_set_size(col, lv_pct(25), lv_pct(95));
    lv_obj_align(col, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    return col;
}

void lvgl_flex_layout_init(lv_display_t *disp) {
    lv_obj_t *scr = lv_display_get_screen_active(disp);

    lv_lock();

    ui_col_0 = flex_col_create_left(scr);
    ui_col_1 = flex_col_create_right(scr);

    lv_unlock();
}
