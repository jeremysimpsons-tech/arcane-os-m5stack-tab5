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

#ifndef LV_SYMBOL_WIFI
#define LV_SYMBOL_WIFI LV_SYMBOL_LIST
#endif

/* ---------- Home: iPhone-like grid (4 cols, vertical scroll) ---------- */
enum { HOME_ICON_COLS = 4, HOME_ICON_GAP = 22, HOME_ICON_PAD = 26 };

static const struct {
    const char *sym;
    const char *label;
    AppScreen   id;
} k_apps[] = {
    { LV_SYMBOL_SETTINGS, "Settings", AppScreen::Settings },
    { LV_SYMBOL_LIST, "Status", AppScreen::Status },
    { LV_SYMBOL_WIFI, "Wi-Fi", AppScreen::WiFi },
    { LV_SYMBOL_DIRECTORY, "Files", AppScreen::Files },
    { LV_SYMBOL_VIDEO, "Camera", AppScreen::Camera },
    { LV_SYMBOL_BARS, "IMU", AppScreen::Imu },
    { LV_SYMBOL_CHARGE, "Power", AppScreen::Power },
    { LV_SYMBOL_DRIVE, "Storage", AppScreen::Sd },
    { LV_SYMBOL_SHUFFLE, "I2C", AppScreen::I2C },
    { LV_SYMBOL_LOOP, "RTC", AppScreen::Rtc },
};

static void home_tile_event(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_PRESSED)
        return;
    const AppScreen id = (AppScreen)(uintptr_t)lv_event_get_user_data(e);
    LvglHal::click_feedback();
    app_show(id);
}

void view_home() {
    shell_mount(APP_NAME, nullptr, nullptr, true);
    lv_obj_t *c = content_ptr();

    color_bg(c, ui_bg_content());
    lv_obj_set_style_radius(c, 0, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(c, false, LV_PART_MAIN);
    lv_obj_set_style_border_width(c, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(c, 0, LV_PART_MAIN);
    /* Extra top padding so first row never tucks under the tall top bar */
    lv_obj_set_style_pad_top(c, HOME_ICON_PAD + 14, LV_PART_MAIN);
    lv_obj_set_style_pad_left(c, HOME_ICON_PAD, LV_PART_MAIN);
    lv_obj_set_style_pad_right(c, HOME_ICON_PAD, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(c, HOME_ICON_PAD, LV_PART_MAIN);
    lv_obj_set_style_margin_top(c, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(c, HOME_ICON_GAP, LV_PART_MAIN);
    lv_obj_set_style_pad_column(c, HOME_ICON_GAP, LV_PART_MAIN);
    lv_obj_set_layout(c, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(c, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_OFF);
    apply_scroll_tabled(c);

    /* Use actual content width so we always hit 4 columns on-device. */
    lv_obj_update_layout(c);
    const int content_w = (int)lv_obj_get_content_width(c);
    /* Slightly smaller blue tiles; icons stay the same size with more inner padding. */
    const int cell = (content_w - HOME_ICON_GAP * (HOME_ICON_COLS - 1)) / HOME_ICON_COLS - 4;
    const int cell_h = cell + 34; /* icon box + label */

    for (unsigned i = 0; i < sizeof(k_apps) / sizeof(k_apps[0]); ++i) {
        lv_obj_t *hit = lv_obj_create(c);
        lv_obj_set_size(hit, (lv_coord_t)cell, (lv_coord_t)cell_h);
        lv_obj_set_style_bg_opa(hit, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(hit, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(hit, 0, LV_PART_MAIN);
        lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
        /* Handle taps even if scroll gesture recognition interferes */
        lv_obj_add_event_cb(hit, home_tile_event, LV_EVENT_PRESSED, (void *)(uintptr_t)k_apps[i].id);
        lv_obj_add_event_cb(hit, home_tile_event, LV_EVENT_CLICKED, (void *)(uintptr_t)k_apps[i].id);
        lv_obj_set_layout(hit, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(hit, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(hit, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(hit, 6, LV_PART_MAIN);

        lv_obj_t *icon_box = lv_obj_create(hit);
        /* Tile smaller than hit cell: keeps glyph away from rounded edges at same 744× scale. */
        lv_obj_set_size(icon_box, (lv_coord_t)(cell - 8), (lv_coord_t)(cell - 8));
        color_bg(icon_box, APP_C_ICON_ACCENT_BLUE);
        lv_obj_set_style_radius(icon_box, APP_RADIUS_TILE, LV_PART_MAIN);
        lv_obj_set_style_border_width(icon_box, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(icon_box, 12, LV_PART_MAIN);
        lv_obj_set_style_clip_corner(icon_box, true, LV_PART_MAIN);
        /* Bubble touch events to the parent hit target */
        lv_obj_add_flag(icon_box, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_remove_flag(icon_box, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *ic = lv_label_create(icon_box);
        lv_label_set_text(ic, k_apps[i].sym);
        lv_obj_set_style_text_font(ic, APP_FONT_HERO, LV_PART_MAIN);
        /* High-contrast glyphs on accent blue (iOS-like tile) */
        lv_obj_set_style_text_color(ic, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_align(ic, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_add_flag(ic, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_center(ic);

        lv_obj_update_layout(ic);
        lv_obj_set_style_transform_pivot_x(ic, lv_obj_get_width(ic) / 2, LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(ic, lv_obj_get_height(ic) / 2, LV_PART_MAIN);
        /* ~1.45× so the symbol fits inside the rounded tile */
        lv_obj_set_style_transform_scale_x(ic, 744, LV_PART_MAIN);
        lv_obj_set_style_transform_scale_y(ic, 744, LV_PART_MAIN);

        lv_obj_t *cap = lv_label_create(hit);
        lv_label_set_text(cap, k_apps[i].label);
        lv_obj_set_style_text_font(cap, APP_FONT_HEADING, LV_PART_MAIN);
        lv_obj_set_style_text_color(cap, lv_color_hex(ui_text_primary()), LV_PART_MAIN);
        lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_letter_space(cap, 1, LV_PART_MAIN);
        lv_label_set_long_mode(cap, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_width(cap, (lv_coord_t)(cell - 4));
        lv_obj_add_flag(cap, LV_OBJ_FLAG_EVENT_BUBBLE);
    }
}
