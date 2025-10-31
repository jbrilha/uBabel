#include "lvgl_tardis_widget.h"
#include "esp_log.h"

#include "comm_manager.h"
#include <stdint.h>
#include <stdio.h>

static const char *SEP = "    |  ";
static const char *TAG = "TaRDIS Widget";
static const char *NOT_CONNECTED = "Not connected";
static const char *NO_IP = "0.0.0.0";

static _lock_t *lvgl_lock = NULL;
static lv_obj_t *notif_box = NULL;
static lv_obj_t *network_box = NULL;
static lv_obj_t *menu = NULL;
static uint8_t selected_node_id[UUID_SIZE] = {0};
static lv_obj_t *shared_device_page = NULL;

typedef struct {
    iot_device_handle_t device_index;
    device_t *device;
    action_t *action;
    parameter_t *parameter;
} menu_item_context_t;

static void apply_focus_style(lv_obj_t *obj, uint32_t color) {
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(obj, lv_palette_main(color), LV_STATE_FOCUSED);
}

#if M5STACK_CORE_BASIC
static lv_group_t *input_group = NULL;

static void menu_page_changed_cb(lv_event_t *e) {
    lv_obj_t *menu_obj = lv_event_get_target(e);

    if (input_group) {
        lv_group_remove_all_objs(input_group);

        lv_obj_t *back_btn = lv_menu_get_main_header_back_button(menu_obj);
        if (back_btn) {
            lv_group_add_obj(input_group, back_btn);
            apply_focus_style(back_btn, LV_PALETTE_RED);
        }

        lv_obj_t *current_page = lv_menu_get_cur_main_page(menu_obj);
        if (current_page) {
            uint32_t child_count = lv_obj_get_child_count(current_page);
            for (uint32_t i = 0; i < child_count; i++) {
                lv_obj_t *cont = lv_obj_get_child(current_page, i);
                lv_group_add_obj(input_group, cont);

                lv_obj_add_flag(cont, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
            }
        }
    }
}

static lv_group_t *tardis_widget_init_input_group(void) {
    lv_group_t *group = lv_group_create();
    lv_group_set_default(group);

    lv_group_set_wrap(group, true);

    return group;
}

void tardis_widget_menu_next(void) {
    if (input_group && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        lv_group_focus_next(input_group);
        _lock_release(lvgl_lock);
    }
}

void tardis_widget_menu_prev(void) {
    if (input_group && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        lv_group_focus_prev(input_group);
        _lock_release(lvgl_lock);
    }
}

void tardis_widget_menu_select(void) {
    if (input_group && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        lv_obj_t *focused = lv_group_get_focused(input_group);
        if (focused) {
            lv_obj_send_event(focused, LV_EVENT_CLICKED, NULL);
        }
        _lock_release(lvgl_lock);
    }
}
#endif

static void parameter_clicked(lv_event_t *e) {
    menu_item_context_t *ctx = (menu_item_context_t *)lv_event_get_user_data(e);
    iot_node_handle_t node = find_node_by_id(selected_node_id);

    if (node != NULL) {
        device_action(node, ctx->device_index, ctx->device, ctx->action,
                      ctx->parameter);
    }
}

static void free_menu_context(lv_event_t *e) {
    menu_item_context_t *ctx = (menu_item_context_t *)lv_event_get_user_data(e);
    free(ctx);
}

static void create_action_subpage(lv_obj_t *menu, lv_obj_t *device_page,
                                  action_t *action,
                                  iot_device_handle_t device_index,
                                  device_t *dev) {

    char page_title[64];
    snprintf(page_title, sizeof(page_title), "%s parameters",
             action->action_name);
    lv_obj_t *action_page = lv_menu_page_create(menu, page_title);

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
                ctx->device_index = device_index;
                ctx->device = dev;
                ctx->action = action;
                ctx->parameter = current_param;

                lv_obj_add_event_cb(cont, parameter_clicked, LV_EVENT_CLICKED,
                                    ctx);
                lv_obj_add_event_cb(cont, free_menu_context, LV_EVENT_DELETE,
                                    ctx);

                apply_focus_style(cont, LV_PALETTE_BLUE);
            }

            current_param = current_param->next;
        } while (current_param != NULL && current_param != first_param);
    }

    lv_obj_t *cont = lv_menu_cont_create(device_page);
    lv_obj_t *label = lv_label_create(cont);
    lv_label_set_text(label, action->action_name);
    lv_menu_set_load_page_event(menu, cont, action_page);

    apply_focus_style(cont, LV_PALETTE_BLUE);
}

static void create_device_subpage(lv_obj_t *menu, lv_obj_t *main_page,
                                  device_t *dev,
                                  iot_device_handle_t device_index) {

    char page_title[64];
    snprintf(page_title, sizeof(page_title), "%s actions", dev->device_name);
    lv_obj_t *device_page = lv_menu_page_create(menu, page_title);

    action_t *current_action = dev->actions;
    if (current_action != NULL) {
        action_t *first_action = current_action;
        do {
            create_action_subpage(menu, device_page, current_action,
                                  device_index, dev);
            current_action = current_action->next;
        } while (current_action != NULL && current_action != first_action);
    }

    lv_obj_t *cont = lv_menu_cont_create(main_page);
    lv_obj_t *label = lv_label_create(cont);
    lv_label_set_text(label, dev->device_name);
    lv_menu_set_load_page_event(menu, cont, device_page);

    apply_focus_style(cont, LV_PALETTE_BLUE);
}

// creating an actual subpage for each node was using too much LVGL mem so
// everything froze so we're using the same shared device page
static void create_shared_device_page(lv_obj_t *menu, device_t *device_info) {
    if (shared_device_page != NULL || device_info == NULL) {
        return;
    }

    shared_device_page = lv_menu_page_create(menu, "Devices");

    device_t *current_device = device_info;
    device_t *first_device = device_info;
    do {
        if (current_device == NULL)
            break;
        create_device_subpage(menu, shared_device_page, current_device, 0);
        current_device = current_device->next;
    } while (current_device != NULL && current_device != first_device);
}

static void node_clicked(lv_event_t *e) {
    uint8_t *node_id = (uint8_t *)lv_event_get_user_data(e);
    memcpy(selected_node_id, node_id, UUID_SIZE);
}

static void create_node_content(lv_obj_t *menu, lv_obj_t *main_page,
                                node_snapshot_t *node_snap) {
    char node_name[64];
    if (memcmp(node_snap->id, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", UUID_SIZE) ==
        0) {
        sprintf(node_name, "ALL NODES");
    } else {
        sprintf(node_name, "Node %.24s...", uuid_to_string(node_snap->id));
    }

    uint8_t *node_id = malloc(UUID_SIZE);
    if (node_id) {
        memcpy(node_id, node_snap->id, UUID_SIZE);

        lv_obj_t *cont = lv_menu_cont_create(main_page);
        lv_obj_t *label = lv_label_create(cont);
        lv_label_set_text(label, node_name);

        lv_obj_add_event_cb(cont, node_clicked, LV_EVENT_CLICKED, node_id);
        lv_obj_add_event_cb(cont, free_menu_context, LV_EVENT_DELETE, node_id);

        lv_menu_set_load_page_event(menu, cont, shared_device_page);

        apply_focus_style(cont, LV_PALETTE_BLUE);
    }
}

lv_obj_t *populate_menu(lv_obj_t *menu, device_t *device_info,
                        node_snapshot_t *nodes, int node_count) {
    lv_obj_t *main_page = (lv_obj_t *)lv_obj_get_user_data(menu);

#ifdef M5STACK_CORE_BASIC
    if (input_group) {
        lv_group_remove_all_objs(input_group);
    }
#endif

    create_shared_device_page(menu, device_info);

    lv_obj_clean(main_page);

    for (int i = 0; i < node_count; i++) {
        create_node_content(menu, main_page, &nodes[i]);
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
#ifdef M5STACK_CORE_BASIC
            lv_obj_send_event(menu, LV_EVENT_VALUE_CHANGED, NULL);
#endif
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
    lv_obj_set_style_pad_right(label, 7, 0);

    lv_obj_t *notif = lv_label_create(box);
    lv_label_set_text(notif, "No notifications");
    lv_label_set_long_mode(notif, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_width(notif, lv_pct(92));

    lv_obj_set_style_border_color(notif, lv_color_hex(0x8080ff), 0);
    lv_obj_set_style_border_width(notif, 2, 0);
    lv_obj_set_style_border_side(notif, LV_BORDER_SIDE_FULL, 0);

    return box;
}

static lv_obj_t *create_network_box(lv_obj_t *container) {
    lv_obj_t *box = lv_obj_create(container);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, lv_obj_get_width(container), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_hor(box, 5, 0);

    lv_obj_t *label = lv_label_create(box);
    lv_label_set_text_static(label, LV_SYMBOL_WIFI);
    lv_obj_set_style_pad_right(label, 2, 0);

    lv_obj_t *text_container = lv_obj_create(box);
    lv_obj_remove_style_all(text_container);
    lv_obj_set_size(text_container, lv_pct(92), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(text_container, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *txt_network = lv_label_create(text_container);
    lv_label_set_text(txt_network, NOT_CONNECTED);
    lv_label_set_long_mode(txt_network, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(txt_network, lv_pct(100));
    lv_obj_set_style_border_color(txt_network, lv_color_hex(0xff8080), 0);
    lv_obj_set_style_border_width(txt_network, 2, 0);
    lv_obj_set_style_border_side(txt_network, LV_BORDER_SIDE_FULL, 0);

    lv_obj_t *txt_ip = lv_label_create(text_container);
    lv_label_set_text(txt_ip, NO_IP);
    lv_label_set_long_mode(txt_ip, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(txt_ip, lv_pct(100));
    lv_obj_set_style_pad_top(txt_ip, 2, 0);
    lv_obj_set_style_border_color(txt_ip, lv_color_hex(0xff8080), 0);
    lv_obj_set_style_border_width(txt_ip, 2, 0);
    lv_obj_set_style_border_side(txt_ip, LV_BORDER_SIDE_FULL, 0);

    return box;
}

void tardis_widget_set_network_up_info(network_event_t *e) {
    if (network_box && lvgl_lock) {
        _lock_acquire(lvgl_lock);

        lv_obj_t *text_container = lv_obj_get_child(network_box, 1);

        lv_obj_t *network_label = lv_obj_get_child(text_container, 0);
        lv_label_set_text(network_label, e->ssid);

        lv_obj_t *ip_label = lv_obj_get_child(text_container, 1);
        lv_label_set_text(ip_label, e->ip);

        lv_obj_set_style_border_color(network_label, lv_color_hex(0x80ff80), 0);
        lv_obj_set_style_border_color(ip_label, lv_color_hex(0x80ff80), 0);

        _lock_release(lvgl_lock);
    }
}

void tardis_widget_set_network_down(void) {
    if (network_box && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        lv_obj_t *text_container = lv_obj_get_child(network_box, 1);

        lv_obj_t *network_label = lv_obj_get_child(text_container, 0);
        lv_label_set_text(network_label, NOT_CONNECTED);

        lv_obj_t *ip_label = lv_obj_get_child(text_container, 1);
        lv_label_set_text(ip_label, NO_IP);

        lv_obj_set_style_border_color(network_label, lv_color_hex(0xff8080), 0);
        lv_obj_set_style_border_color(ip_label, lv_color_hex(0xff8080), 0);
        _lock_release(lvgl_lock);
    }
}

void tardis_widget_set_notif_txt(const char *notif) {
    if (notif_box && lvgl_lock) {
        _lock_acquire(lvgl_lock);
        // second child, based on creation order!!
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

            lv_obj_set_width(menu, lv_pct(100));

            // empty main page, set later once a node is detected
            lv_obj_t *main_page = lv_menu_page_create(menu, NULL);
            lv_menu_set_page(menu, main_page);

            lv_obj_set_user_data(menu, main_page);

#ifdef M5STACK_CORE_BASIC
            input_group = tardis_widget_init_input_group();
            lv_obj_add_event_cb(menu, menu_page_changed_cb,
                                LV_EVENT_VALUE_CHANGED, NULL);
#endif
        }

        if (!network_box) {
            network_box = create_network_box(col_box);
        }

        if (!notif_box) {
            notif_box = create_notif_box(col_box);
        }

        _lock_release(lvgl_lock);
    }
}
