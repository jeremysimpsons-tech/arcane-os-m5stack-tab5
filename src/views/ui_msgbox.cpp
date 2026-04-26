#include "views_internal.hpp"
#include "arcane_lvgl.h"

void ui_mbox_bind_children_clicked(lv_obj_t *btn, lv_event_cb_t cb, void *ud) {
    if (!btn || !cb) {
        return;
    }
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, ud);
    const uint32_t n = lv_obj_get_child_count(btn);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *ch = lv_obj_get_child(btn, i);
        if (ch) {
            lv_obj_add_event_cb(ch, cb, LV_EVENT_CLICKED, ud);
        }
    }
}

static void ui_msgbox_backdrop_click_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *mb   = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_t *t    = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t *bd   = mb ? lv_obj_get_parent(mb) : nullptr;
    if (!mb || !t || !bd || t != bd) {
        return;
    }
    lv_msgbox_close(mb);
}

void ui_msgbox_install_backdrop_dismiss(lv_obj_t *mb) {
    if (!mb) {
        return;
    }
    lv_obj_t *bd = lv_obj_get_parent(mb);
    if (!bd || bd == lv_layer_top()) {
        return;
    }
    lv_obj_add_event_cb(bd, ui_msgbox_backdrop_click_cb, LV_EVENT_CLICKED, mb);
}

void ui_msgbox_add_close_and_backdrop(lv_obj_t *mb) {
    if (!mb) {
        return;
    }
    lv_msgbox_add_close_button(mb);
    ui_msgbox_install_backdrop_dismiss(mb);
}
