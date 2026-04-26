#include "Views.hpp"
#include "AppTypes.hpp"
#include "LvglHal.hpp"
#include "views_config.hpp"
#include "views_internal.hpp"
#include "app_config.h"
#include "app_theme.hpp"
#include "arcane_lvgl.h"
#include <M5Unified.h>
#include <Arduino.h>


/* placeholder pages */
void views_noop_right(lv_event_t *e) { (void)e; }

static void files_mbox_dismiss_from_footer_cb(lv_event_t *e) {
    (void)lv_event_get_target(e);
    LvglHal::click_feedback();
    lv_obj_t *mb = (lv_obj_t *)lv_event_get_user_data(e);
    if (mb && lv_obj_is_valid(mb)) {
        lv_msgbox_close(mb);
    }
}

void files_action_sheet_cb(lv_event_t *e) {
    (void)e;
    LvglHal::click_feedback();
    lv_obj_t *mb = lv_msgbox_create(nullptr);
    lv_msgbox_add_title(mb, "File action");
    lv_msgbox_add_text(mb, "Open / share / delete — connect FS + actions here.");
    lv_obj_t *b1 = lv_msgbox_add_footer_button(mb, "Open");
    lv_obj_t *b2 = lv_msgbox_add_footer_button(mb, "Delete");
    ui_mbox_bind_children_clicked(b1, files_mbox_dismiss_from_footer_cb, mb);
    ui_mbox_bind_children_clicked(b2, files_mbox_dismiss_from_footer_cb, mb);
    ui_msgbox_add_close_and_backdrop(mb);
    lv_obj_center(mb);
}

void view_files(void *user_ctx) {
    (void)user_ctx;
    shell_mount("Files", files_action_sheet_cb, "···", false);
    lv_obj_t *c = content_ptr();

    lv_obj_t *path = lv_label_create(c);
    lv_label_set_text(path, "/  internal");
    lv_obj_set_style_text_font(path, APP_FONT_CAP, LV_PART_MAIN);
    lv_obj_set_style_text_color(path, lv_color_hex(APP_C_ICON_ACCENT_BLUE), LV_PART_MAIN);

    lv_obj_t *row = lv_obj_create(c);
    /* Taller selector so both items are fully visible on-device. */
    lv_obj_set_size(row, lv_pct(100), 96);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 8, LV_PART_MAIN);
    app_style_muted_card(row);
    for (const char *t : { "Internal", "SD card" }) {
        lv_obj_t *b = lv_button_create(row);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_set_height(b, 72);
        shell_style_topbar_btn(b);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, t);
        lv_obj_set_style_text_font(l, APP_FONT_SUB, LV_PART_MAIN);
        lv_obj_set_style_text_color(l, lv_color_hex(APP_C_ICON_GREY_DARK), LV_PART_MAIN);
        lv_obj_center(l);
    }

    lv_obj_t *list = lv_list_create(c);
    lv_obj_set_width(list, lv_pct(100));
    lv_obj_set_flex_grow(list, 1);
    apply_scroll_tabled(list);
    color_bg(list, ui_bg_content());
    lv_obj_set_style_radius(list, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 4, LV_PART_MAIN);
    lv_obj_set_style_border_color(list, lv_color_hex(0xE5E5EA), LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 1, LV_PART_MAIN);
    const char *items[] = { "docs/", "photo.jpg", "notes.txt", "archive.zip" };
    for (unsigned i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
        lv_list_add_button(list, nullptr, items[i]);
    }
}
