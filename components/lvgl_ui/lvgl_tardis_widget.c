#include "lvgl_tardis_widget.h"
#include "core/lv_obj_pos.h"
#include "core/lv_obj_style_gen.h"
#include "esp_log.h"

#include "lora_types.h"
#include "misc/lv_area.h"
#include "widgets/label/lv_label.h"
#include <stdio.h>

#define RSSI_BAR_MIN (-157)
#define RSSI_BAR_MAX (-30)

#define SNR_BAR_MIN (-100)
#define SNR_BAR_MAX (200)

// #define BAR_WIDTH 280
#define BAR_WIDTH 260
#define BAR_HEIGHT 15

#define ANIM_DURATION 2000

static const char *SEP = "    |  ";

static _lock_t *lvgl_lock = NULL;
static lv_obj_t *notif_box = NULL;
static lv_obj_t *menu = NULL;

static lv_obj_t *create_menu(lv_obj_t *container) {
    lv_obj_t *menu = lv_menu_create(container);
    lv_obj_set_flex_grow(menu, 1); // menu expands in flex parent


    lv_obj_center(menu);

    // /*Modify the header*/
    // lv_obj_t *back_btn = lv_menu_get_main_header_back_button(menu);
    // lv_obj_t *back_button_label = lv_label_create(back_btn);
    // lv_label_set_text(back_button_label, "Back");

    lv_obj_t *cont;
    lv_obj_t *label;

    /*Create sub pages*/
    lv_obj_t *sub_1_page = lv_menu_page_create(menu, "Page 1");

    cont = lv_menu_cont_create(sub_1_page);
    label = lv_label_create(cont);
    lv_label_set_text(label, "Hello 1");

    lv_obj_t *sub_2_page = lv_menu_page_create(menu, "Page 2");

    cont = lv_menu_cont_create(sub_2_page);
    label = lv_label_create(cont);
    lv_label_set_text(label, "Hello 2");

    lv_obj_t *sub_3_page = lv_menu_page_create(menu, "Page 3");

    cont = lv_menu_cont_create(sub_3_page);
    label = lv_label_create(cont);
    lv_label_set_text(label, "Hello 1");

    /*Create a main page*/
    lv_obj_t *main_page = lv_menu_page_create(menu, NULL);


    cont = lv_menu_cont_create(main_page);
    label = lv_label_create(cont);
    lv_label_set_text(label, "Item 1 (Click me!)");
    lv_menu_set_load_page_event(menu, cont, sub_1_page);

    cont = lv_menu_cont_create(main_page);
    label = lv_label_create(cont);
    lv_label_set_text(label, "Item 2 (Click me!)");
    lv_menu_set_load_page_event(menu, cont, sub_2_page);

    cont = lv_menu_cont_create(main_page);
    label = lv_label_create(cont);
    lv_label_set_text(label, "Item 3 (Click me!)");
    lv_menu_set_load_page_event(menu, cont, sub_3_page);

    lv_menu_set_page(menu, main_page);

    return menu;
}

static lv_obj_t *create_notif_box(lv_obj_t *container) {
    lv_obj_t *box = lv_obj_create(container);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, lv_obj_get_width(container), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(box, 5, 0);

    lv_obj_t *label = lv_label_create(box);
    lv_label_set_text_static(label, LV_SYMBOL_BELL);
    lv_obj_set_style_pad_right(label, 5, 0);

    lv_obj_t *notif = lv_label_create(box);
    lv_label_set_text(notif, "No notifications");
    lv_label_set_long_mode(notif, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_width(notif, lv_pct(95));

    lv_obj_set_style_border_color(notif, lv_color_hex(0x8080ff), 0);
    lv_obj_set_style_border_width(notif, 2, 0);
    lv_obj_set_style_border_side(notif, LV_BORDER_SIDE_FULL, 0);

    return box;
}

void tardis_widget_set_notif_txt(const char *notif) {
    if (notif_box && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        // second child, based on cration order!!
        lv_obj_t *notif_label = lv_obj_get_child(notif_box, 1);
        lv_label_set_text_fmt(notif_label, "%s%s", notif, SEP);
        _lock_release(lvgl_lock);
    }
}

void tardis_widget_init(lv_display_t *disp, _lock_t *lock) {
    lv_obj_t *scr = lv_display_get_screen_active(disp);

    tardis_widget_init_on_container(scr, lock);
}

void tardis_widget_init_on_container(lv_obj_t *container, _lock_t *lock) {
    lvgl_lock = lock;

    if (lvgl_lock) {
        _lock_acquire(lvgl_lock);
        // lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t *col_box = lv_obj_create(container);
        lv_obj_remove_style_all(col_box);
        lv_obj_set_size(col_box, lv_obj_get_width(container),
                        lv_obj_get_height(container));

        // lv_obj_align(col_box, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_set_flex_flow(col_box, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);
        // lv_obj_set_style_pad_gap(col_box, 5, 0);

        if (!menu) {
            menu = create_menu(col_box);
        }

        if (!notif_box) {
            notif_box = create_notif_box(col_box);
        }

        _lock_release(lvgl_lock);
    }
}
