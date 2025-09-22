#include "lvgl_tardis_widget.h"
#include "esp_log.h"

#include "comm_manager.h"
#include <stdint.h>
#include <stdio.h>

static const char *SEP = "    |  ";
static const char *TAG = "TaRDIS Widget";

static _lock_t *lvgl_lock = NULL;
static lv_obj_t *notif_box = NULL;
static lv_obj_t *print_box = NULL;
static lv_obj_t *menu = NULL;

typedef struct {
    uint8_t node_id[UUID_SIZE];
    iot_device_handle_t device_index;
    device_t *device_spec;
    action_t *action;
    parameter_t *parameter;
} menu_item_context_t;

static void parameter_clicked(lv_event_t *e) {
    menu_item_context_t *ctx = (menu_item_context_t *)lv_event_get_user_data(e);

    iot_node_handle_t node = find_node_by_id(ctx->node_id);

    if (node != NULL) {
        device_action(node, ctx->device_index, ctx->device_spec, ctx->action,
                      ctx->parameter);
    }
}

static void free_menu_context(lv_event_t *e) {
    menu_item_context_t *ctx = (menu_item_context_t *)lv_event_get_user_data(e);
    free(ctx);
}
static void create_action_subpage(lv_obj_t *menu, lv_obj_t *device_page,
                                  action_t *action, uint8_t *node_id,
                                  iot_device_handle_t device_index,
                                  device_t *device_spec) {

    lv_obj_t *action_page = lv_menu_page_create(menu, action->action_name);

    parameter_t *current_param = action->parameters;
    if (current_param != NULL) {
        parameter_t *first_param = current_param;
        do {
            lv_obj_t *cont = lv_menu_cont_create(action_page);
            lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_t *label = lv_label_create(cont);
            lv_label_set_text(label, current_param->parameter_name);

            menu_item_context_t *ctx = malloc(sizeof(menu_item_context_t));
            if (ctx != NULL) {
                memcpy(ctx->node_id, node_id, UUID_SIZE);
                ctx->device_index = device_index;
                ctx->device_spec = device_spec;
                ctx->action = action;
                ctx->parameter = current_param;

                lv_obj_add_event_cb(cont, parameter_clicked, LV_EVENT_CLICKED,
                                    ctx);
                lv_obj_add_event_cb(cont, free_menu_context, LV_EVENT_DELETE,
                                    ctx);
            }

            current_param = current_param->next;
        } while (current_param != NULL && current_param != first_param);
    }

    lv_obj_t *cont = lv_menu_cont_create(device_page);
    lv_obj_t *label = lv_label_create(cont);
    lv_label_set_text(label, action->action_name);
    lv_menu_set_load_page_event(menu, cont, action_page);
}

static void create_device_subpage(lv_obj_t *menu, lv_obj_t *main_page,
                                  device_t *dev, uint8_t *node_id,
                                  iot_device_handle_t device_index) {

    lv_obj_t *device_page = lv_menu_page_create(menu, dev->device_name);

    action_t *current_action = dev->actions;
    if (current_action != NULL) {
        action_t *first_action = current_action;
        do {
            create_action_subpage(menu, device_page, current_action, node_id,
                                  device_index, dev);
            current_action = current_action->next;
        } while (current_action != NULL && current_action != first_action);
    }

    lv_obj_t *cont = lv_menu_cont_create(main_page);
    lv_obj_t *label = lv_label_create(cont);
    lv_label_set_text(label, dev->device_name);
    lv_menu_set_load_page_event(menu, cont, device_page);
}

static void create_node_subpage(lv_obj_t *menu, lv_obj_t *main_page,
                                node_snapshot_t *node_snap,
                                device_t *device_info) {

    char node_name[64];
    if (memcmp(node_snap->id, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", UUID_SIZE) ==
        0) {
        sprintf(node_name, "ALL NODES");
    } else {
        sprintf(node_name, "Node %.16s...", uuid_to_string(node_snap->id));
    }

    lv_obj_t *node_page = lv_menu_page_create(menu, node_name);

    device_t *current_device = device_info;
    device_t *first_device = device_info;
    do {
        if (current_device == NULL)
            break;
        create_device_subpage(menu, node_page, current_device, node_snap->id,
                              0);
        current_device = current_device->next;
    } while (current_device != NULL && current_device != first_device);

    lv_obj_t *cont = lv_menu_cont_create(main_page);
    lv_obj_t *label = lv_label_create(cont);
    lv_label_set_text(label, node_name);
    lv_menu_set_load_page_event(menu, cont, node_page);
}

lv_obj_t *populate_menu(lv_obj_t *menu, device_t *device_info,
                        node_snapshot_t *nodes, int node_count) {
    lv_obj_t *main_page = (lv_obj_t *)lv_obj_get_user_data(menu);

    lv_obj_clean(main_page);

    for (int i = 0; i < node_count; i++) {
        create_node_subpage(menu, main_page, &nodes[i], device_info);
    }

    return menu;
}

void tardis_widget_populate_menu(void) {
    if (menu) {
        device_t *device_info = get_device_info_data();
        node_snapshot_t *nodes = NULL;
        int node_count = get_nodes_snapshot(&nodes);

        if (lvgl_lock) {
            _lock_acquire(lvgl_lock);
            populate_menu(menu, device_info, nodes, node_count);
            _lock_release(lvgl_lock);
        }

        for (int i = 0; i < node_count; i++) {
            free(nodes[i].devices);
        }
        free(nodes);
    }
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

static lv_obj_t *create_print_box(lv_obj_t *container) {
    lv_obj_t *box = lv_obj_create(container);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, lv_obj_get_width(container), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(box, 5, 0);

    lv_obj_t *label = lv_label_create(box);
    lv_label_set_text_static(label, LV_SYMBOL_WIFI);
    lv_obj_set_style_pad_right(label, 5, 0);

    lv_obj_t *txt = lv_label_create(box);
    lv_label_set_text(txt, "Not connected");
    lv_label_set_long_mode(txt, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_width(txt, lv_pct(95));

    lv_obj_set_style_border_color(txt, lv_color_hex(0x80ff80), 0);
    lv_obj_set_style_border_width(txt, 2, 0);
    lv_obj_set_style_border_side(txt, LV_BORDER_SIDE_FULL, 0);

    return box;
}

void tardis_widget_set_network_info(network_event_t *e) {
// TODO TODO TODO

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

void tardis_widget_set_print_txt(const char *txt) {
    if (print_box && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        // second child, based on cration order!!
        lv_obj_t *txt_label = lv_obj_get_child(print_box, 1);
        lv_label_set_text_fmt(txt_label, "%s%s", txt, SEP);
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

        lv_obj_t *col_box = lv_obj_create(container);
        lv_obj_remove_style_all(col_box);
        lv_obj_set_size(col_box, lv_obj_get_width(container),
                        lv_obj_get_height(container));

        lv_obj_set_flex_flow(col_box, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_START);

        if (!menu) {
            menu = lv_menu_create(col_box);
            lv_obj_set_flex_grow(menu, 1);
            lv_obj_center(menu);

            // empty main page, set later once a node is detected
            lv_obj_t *main_page = lv_menu_page_create(menu, NULL);
            lv_menu_set_page(menu, main_page);

            lv_obj_set_user_data(menu, main_page);
        }

        if (!print_box) {
            print_box = create_print_box(col_box);
        }

        if (!notif_box) {
            notif_box = create_notif_box(col_box);
        }

        _lock_release(lvgl_lock);
    }
}
