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
#include <stdio.h>
#include <string.h>

extern "C" const lv_img_dsc_t wolf;


/* ---------- full-screen chrome-free splash (dismiss on touch only; no timer) ---------- */
/* s_splash_dismissed: views_state */

/** Splash content fade duration (ms). */
static constexpr uint32_t SPLASH_FADE_MS = 1800u;
/** Startup bar shuttle: one full left→right→left cycle (slower than fade; loops until splash dismissed). */
static constexpr uint32_t SPLASH_BAR_CYCLE_MS = 3200u;

static void splash_boot_bar_anim(lv_obj_t *seg, lv_coord_t x_min, lv_coord_t x_max) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, seg);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&a, x_min, x_max);
    lv_anim_set_duration(&a, SPLASH_BAR_CYCLE_MS / 2u);
    lv_anim_set_playback_time(&a, SPLASH_BAR_CYCLE_MS / 2u);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void splash_dismiss_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_PRESSED)
        return;
    if (s_splash_dismissed)
        return;
    s_splash_dismissed = true;
    LvglHal::click_feedback();
    app_show(AppScreen::Home);
}

static void splash_add_dismiss(lv_obj_t *o) {
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(o, splash_dismiss_cb, LV_EVENT_PRESSED, nullptr);
}

static void splash_fade_in(lv_obj_t *o, uint32_t dur_ms) {
    if (!o || !lv_obj_is_valid(o))
        return;
    lv_obj_set_style_opa(o, LV_OPA_0, LV_PART_MAIN);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_values(&a, LV_OPA_0, LV_OPA_COVER);
    lv_anim_set_duration(&a, dur_ms);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)[](void *obj, int32_t v) {
        lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, LV_PART_MAIN);
    });
    lv_anim_start(&a);
}

void view_splash() {
    s_splash_dismissed = false;
    lv_obj_t *scr = lv_screen_active();
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clean(scr);
    color_bg(scr, APP_C_SPLASH_BG);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);

    /* Centered splash mark: wolf image + ARCANE OS title + subtitle */
    lv_obj_t *mark = lv_obj_create(scr);
    lv_obj_set_size(mark, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(mark, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(mark, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(mark, 0, LV_PART_MAIN);
    lv_obj_set_layout(mark, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(mark, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mark, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(mark, 18, LV_PART_MAIN);
    lv_obj_center(mark);
    lv_obj_set_y(mark, lv_obj_get_y(mark) - 80);

    lv_obj_t *wim = lv_image_create(mark);
    lv_image_set_src(wim, &wolf);

    lv_obj_t *title = lv_label_create(mark);
    lv_label_set_text(title, APP_NAME);
    lv_obj_set_style_text_font(title, APP_FONT_HERO, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    lv_obj_t *ver = lv_label_create(mark);
    lv_label_set_text(ver, "Home Base Edition");
    lv_obj_set_style_text_font(ver, APP_FONT_SUB, LV_PART_MAIN);
    lv_obj_set_style_text_color(ver, lv_color_hex(0xA8A8AE), LV_PART_MAIN);

    /* macOS-style startup bar: pill track + lighter segment shuttling with the fade. */
    constexpr lv_coord_t bar_track_w = 300;
    constexpr lv_coord_t bar_track_h = 6;
    constexpr lv_coord_t bar_seg_w   = 88;
    constexpr lv_coord_t bar_seg_h   = 4;
    constexpr lv_coord_t bar_pad_x   = 3;

    lv_obj_t *bar_track = lv_obj_create(mark);
    lv_obj_set_size(bar_track, bar_track_w, bar_track_h);
    lv_obj_set_style_bg_opa(bar_track, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_track, lv_color_hex(0x3A3A3C), LV_PART_MAIN);
    lv_obj_set_style_radius(bar_track, bar_track_h / 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_track, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar_track, 0, LV_PART_MAIN);
    lv_obj_remove_flag(bar_track, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bar_seg = lv_obj_create(bar_track);
    lv_obj_set_size(bar_seg, bar_seg_w, bar_seg_h);
    lv_obj_set_pos(bar_seg, bar_pad_x, (bar_track_h - bar_seg_h) / 2);
    lv_obj_set_style_bg_opa(bar_seg, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_seg, lv_color_hex(0xD1D1D6), LV_PART_MAIN);
    lv_obj_set_style_radius(bar_seg, (bar_seg_h + 1) / 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_seg, 0, LV_PART_MAIN);
    lv_obj_remove_flag(bar_seg, LV_OBJ_FLAG_SCROLLABLE);

    splash_boot_bar_anim(bar_seg, bar_pad_x, bar_track_w - bar_seg_w - bar_pad_x);

    splash_fade_in(mark, SPLASH_FADE_MS);

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "Touch screen to continue");
    lv_obj_set_style_text_font(hint, APP_FONT_HEADING, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(APP_C_ACCENT), LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -48);

    splash_add_dismiss(scr);
    splash_add_dismiss(mark);
    splash_add_dismiss(wim);
    splash_add_dismiss(title);
    splash_add_dismiss(ver);
    splash_add_dismiss(bar_track);
    splash_add_dismiss(bar_seg);
    splash_add_dismiss(hint);
}
