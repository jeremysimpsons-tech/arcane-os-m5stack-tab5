/* Shell + pages: scroll physics, top bar, splash without chrome, no duplicate headers. */
#include "Views.hpp"
#include "AppTypes.hpp"
#include "LvglHal.hpp"
#include "TouchCal.hpp"
#include "app_config.h"
#include "app_theme.hpp"
#include "arcane_lvgl.h"
#include <M5Unified.h>
#include <Arduino.h>
#include <Preferences.h>
#include <stdio.h>
#include <esp_system.h>

#define ARC_SETTINGS_NVS   "arc_set"
#define ARC_SETTINGS_MAGIC 0x53415431u /* 'SAT1' */

extern "C" const lv_img_dsc_t wolf;

static lv_obj_t *s_cal_lbl = nullptr;
static lv_obj_t *s_cal_dot = nullptr;

static void apply_scroll_tabled(lv_obj_t *o) {
    lv_obj_add_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_flag(o, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_AUTO);
}

static void color_bg(lv_obj_t *o, uint32_t hex) {
    lv_obj_set_style_bg_color(o, lv_color_hex(hex), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
}

/* ---------- full-screen chrome-free splash (dismiss on touch only; no timer) ---------- */
static bool s_splash_dismissed = false;

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

/* ---------- 4-tap touch calibration (raw from M5, stored min/max) ---------- */
static int s_step;
static int32_t s_rx[4], s_ry[4];
static bool s_cal_goes_home = false;

static void cal_render_target() {
    lv_obj_t  *scr = lv_screen_active();
    const int  m  = 56;
    int        cx = 0, cy = 0;
    switch (s_step) {
    case 0: cx = m; cy = m; break;
    case 1: cx = APP_LCD_WIDTH - 1 - m; cy = m; break;
    case 2: cx = m; cy = APP_LCD_HEIGHT - 1 - m; break;
    case 3: cx = APP_LCD_WIDTH - 1 - m; cy = APP_LCD_HEIGHT - 1 - m; break;
    default: break;
    }
    if (!s_cal_dot) {
        s_cal_dot = lv_obj_create(scr);
        lv_obj_set_size(s_cal_dot, 32, 32);
        color_bg(s_cal_dot, APP_C_ACCENT);
        lv_obj_set_style_radius(s_cal_dot, 16, LV_PART_MAIN);
    }
    lv_obj_set_pos(s_cal_dot, cx - 16, cy - 16);
    lv_obj_move_foreground(s_cal_dot);
}

static void cal_inp_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_PRESSED)
        return;
    (void)lv_event_get_target(e);
    M5.update();
    lgfx::touch_point_t tp[1];
    if (M5.Display.getTouch(tp, 1) < 1)
        return;
    if (s_step >= 4)
        return;
    s_rx[s_step] = (int32_t)tp[0].x;
    s_ry[s_step] = (int32_t)tp[0].y;
    s_step++;
    if (s_step >= 4) {
        int32_t xmin = s_rx[0], xmax = s_rx[0], ymin = s_ry[0], ymax = s_ry[0];
        for (int i = 1; i < 4; ++i) {
            if (s_rx[i] < xmin) xmin = s_rx[i];
            if (s_rx[i] > xmax) xmax = s_rx[i];
            if (s_ry[i] < ymin) ymin = s_ry[i];
            if (s_ry[i] > ymax) ymax = s_ry[i];
        }
        TouchCalData d;
        d.xmin  = xmin;
        d.xmax  = xmax;
        d.ymin  = ymin;
        d.ymax  = ymax;
        d.valid = true;
        if (TouchCalStore::save(d)) {
            LvglHal::set_touch_cal(d);
            LvglHal::click_feedback();
        }
        app_show(s_cal_goes_home ? AppScreen::Home : AppScreen::Splash);
        return;
    }
    LvglHal::click_feedback();
    if (s_cal_lbl) {
        char t[64];
        snprintf(t, sizeof(t), "Tap the target  (%d / 4)", s_step + 1);
        lv_label_set_text(s_cal_lbl, t);
    }
    cal_render_target();
}

static void view_calibrate_impl() {
    s_step     = 0;
    s_cal_dot  = nullptr;
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    color_bg(scr, APP_C_BG);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    s_cal_lbl = lv_label_create(scr);
    lv_label_set_text(s_cal_lbl, "Tap the target  (1 / 4)");
    lv_obj_set_style_text_font(s_cal_lbl, APP_FONT_SUB, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_cal_lbl, lv_color_hex(APP_C_TEXT), LV_PART_MAIN);
    lv_obj_align(s_cal_lbl, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t *catchr = lv_obj_create(scr);
    lv_obj_set_size(catchr, APP_LCD_WIDTH, APP_LCD_HEIGHT);
    lv_obj_align(catchr, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(catchr, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(catchr, 0, LV_PART_MAIN);
    lv_obj_add_flag(catchr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(catchr, cal_inp_cb, LV_EVENT_PRESSED, nullptr);
    cal_render_target();
}

void view_calibrate() {
    s_cal_goes_home = false;
    view_calibrate_impl();
}

void view_touch_calibrate() {
    s_cal_goes_home = true;
    view_calibrate_impl();
}

/* ---------- global shell: top band + content (no second header) ---------- */
static lv_obj_t *s_cbody = nullptr;

static void sh_home_event(lv_event_t *e) {
    (void)e;
    LvglHal::click_feedback();
    app_show(AppScreen::Home);
}

static void sh_back_event(lv_event_t *e) {
    (void)e;
    LvglHal::click_feedback();
    app_show(AppScreen::Home);
}

static void sh_backhome_event(lv_event_t *e) { sh_back_event(e); }

static void sh_power_menu_event(lv_event_t *e) {
    (void)e;
    LvglHal::click_feedback();
    app_show(AppScreen::PowerMenu);
}

static void power_off_do_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
        return;
    LvglHal::click_feedback();
    M5.update();
    M5.Power.powerOff();
}

static void sh_right_event(lv_event_t *e) {
    LvglHal::click_feedback();
    void (*fn)(lv_event_t *) = (void (*)(lv_event_t *))lv_event_get_user_data(e);
    if (fn) fn(e);
}

static lv_timer_t *s_shell_batt_timer = nullptr;

static const char *shell_battery_symbol(int pct) {
    if (pct >= 88)
        return LV_SYMBOL_BATTERY_FULL;
    if (pct >= 66)
        return LV_SYMBOL_BATTERY_3;
    if (pct >= 44)
        return LV_SYMBOL_BATTERY_2;
    if (pct >= 22)
        return LV_SYMBOL_BATTERY_1;
    return LV_SYMBOL_BATTERY_EMPTY;
}

static void shell_batt_apply_color2(lv_obj_t *sym, lv_obj_t *num, int pct, bool charging) {
    /* Smartphone-style: red (low) → orange (mid) → green (high / charging). */
    uint32_t hex = 0x34C759; /* good */
    if (charging) {
        hex = 0x34C759;
    } else if (pct >= 0 && pct <= 100) {
        if (pct <= 20)
            hex = 0xFF453A;
        else if (pct <= 50)
            hex = 0xFF9F0A;
    }
    const lv_color_t c = lv_color_hex(hex);
    lv_obj_set_style_text_color(sym, c, LV_PART_MAIN);
    lv_obj_set_style_text_color(num, c, LV_PART_MAIN);
}

struct ShellBatt {
    lv_obj_t *chrg; /* lightning; left of battery; visible only when charging */
    lv_obj_t *sym;
    lv_obj_t *num;
};
static ShellBatt s_shell_batt = {};

static void shell_batt_refresh_all() {
    lv_obj_t *chrg = s_shell_batt.chrg;
    lv_obj_t *sym  = s_shell_batt.sym;
    lv_obj_t *num  = s_shell_batt.num;
    if (!sym || !num || !lv_obj_is_valid(sym) || !lv_obj_is_valid(num))
        return;
    M5.update();
    int         pct = (int)M5.Power.getBatteryLevel();
    const m5::Power_Class::is_charging_t chg = M5.Power.isCharging();
    bool        charging                    = (chg == m5::Power_Class::is_charging);
    if (!charging && chg == m5::Power_Class::charge_unknown) {
        const int vbus = (int)M5.Power.getVBUSVoltage();
        if (vbus > 4200)
            charging = true;
    }
    if (chrg) {
        if (charging && lv_obj_is_valid(chrg)) {
            lv_label_set_text(chrg, LV_SYMBOL_CHARGE);
            lv_obj_set_style_text_color(chrg, lv_color_hex(0x34C759), LV_PART_MAIN);
            lv_obj_remove_flag(chrg, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(chrg, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (pct < 0 || pct > 100) {
        lv_label_set_text(sym, LV_SYMBOL_BATTERY_FULL);
        lv_label_set_text(num, " —");
        const lv_color_t c = lv_color_hex(0xD1D1D6);
        lv_obj_set_style_text_color(sym, c, LV_PART_MAIN);
        lv_obj_set_style_text_color(num, c, LV_PART_MAIN);
    } else {
        lv_label_set_text(sym, shell_battery_symbol(pct));
        char nb[20];
        snprintf(nb, sizeof(nb), " %d%%", pct);
        lv_label_set_text(num, nb);
        shell_batt_apply_color2(sym, num, pct, charging);
    }
}

static void shell_batt_timer_cb(lv_timer_t *t) {
    (void)t;
    shell_batt_refresh_all();
}

static void shell_style_topbar_btn(lv_obj_t *btn) {
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN);
}

/* ---------- Backlight timeout (Settings) ---------- */
static uint32_t   s_backlight_timeout_ms = 0; /* 0 = disabled */
static uint32_t   s_dim_timeout_ms       = 0; /* 0 = disabled; soft-dim to 10% of user brightness */
static uint32_t   s_last_activity_ms     = 0;
static bool       s_backlight_dimmed     = false; /* full off */
static bool       s_soft_dimmed            = false; /* 10% of restore level */
static uint8_t    s_backlight_restore    = 60; /* percent */
static lv_timer_t *s_idle_timer          = nullptr;

static void user_activity_poke() {
    s_last_activity_ms = (uint32_t)lv_tick_get();
    if (s_backlight_dimmed) {
        M5.Display.setBrightness((uint8_t)((255u * (uint32_t)s_backlight_restore) / 100u));
        s_backlight_dimmed = false;
        s_soft_dimmed      = false;
    } else if (s_soft_dimmed) {
        M5.Display.setBrightness((uint8_t)((255u * (uint32_t)s_backlight_restore) / 100u));
        s_soft_dimmed = false;
    }
}

void arc_notify_pointer_activity(void) {
    user_activity_poke();
}

static void idle_timer_cb(lv_timer_t *t) {
    (void)t;
    if (!s_backlight_timeout_ms && !s_dim_timeout_ms)
        return;
    const uint32_t now  = (uint32_t)lv_tick_get();
    const uint32_t idle = now - s_last_activity_ms;

    /* Full backlight off (after dim if both are enabled). */
    if (s_backlight_timeout_ms != 0 && idle >= s_backlight_timeout_ms) {
        if (!s_backlight_dimmed) {
            s_backlight_dimmed = true;
            s_soft_dimmed      = false;
            M5.Display.setBrightness(0);
        }
        return;
    }

    /* Soft dim to 10% of the user’s brightness level (not full black). */
    if (s_dim_timeout_ms != 0 && idle >= s_dim_timeout_ms) {
        if (!s_backlight_dimmed && !s_soft_dimmed) {
            s_soft_dimmed = true;
            const uint32_t full = (255u * (uint32_t)s_backlight_restore) / 100u;
            uint32_t dim = full * 10u / 100u;
            if (dim < 1u)
                dim = 1u;
            if (full >= 2u && dim >= full)
                dim = full - 1u;
            M5.Display.setBrightness((uint8_t)dim);
        }
    }
}

/* In-app home glyph: 2× scale; pivot at label center. */
static void home_icon_center_pivot(lv_obj_t *lbl) {
    const lv_coord_t w = lv_obj_get_width(lbl);
    const lv_coord_t h = lv_obj_get_height(lbl);
    if (w <= 0 || h <= 0)
        return;
    lv_obj_set_style_transform_pivot_x(lbl, w / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(lbl, h / 2, LV_PART_MAIN);
}

static void home_icon_size_cb(lv_event_t *e) {
    home_icon_center_pivot((lv_obj_t *)lv_event_get_target(e));
}

void shell_mount(const char *title, void (*on_right)(lv_event_t *), const char *right_caption, bool home_shell) {
    (void)title;
    if (s_shell_batt_timer) {
        lv_timer_delete(s_shell_batt_timer);
        s_shell_batt_timer = nullptr;
    }

    lv_obj_t *scr = lv_screen_active();
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clean(scr);
    /* Match top bar so no off-tone strip shows if flex leaves a subpixel gap */
    color_bg(scr, APP_C_TOP);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(scr, 0, LV_PART_MAIN);
    lv_obj_set_layout(scr, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *s_bar = lv_obj_create(scr);
    lv_obj_set_size(s_bar, APP_LCD_WIDTH, APP_TOPBAR_PX);
    color_bg(s_bar, APP_C_TOP);
    lv_obj_set_style_pad_all(s_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_bar, 0, LV_PART_MAIN);
    lv_obj_set_layout(s_bar, LV_LAYOUT_NONE);
    lv_obj_remove_flag(s_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Slightly larger side button improves in-app home icon legibility. */
    const lv_coord_t side_btn = (lv_coord_t)(APP_TOPBAR_PX - 14);

    /* Left: home = logo placeholder; in-app = back (→ Home) */
    if (home_shell) {
        lv_obj_t *logo = lv_obj_create(s_bar);
        lv_obj_set_size(logo, side_btn, side_btn);
        lv_obj_set_style_radius(logo, 0, LV_PART_MAIN);
        color_bg(logo, APP_C_TOP);
        lv_obj_set_style_border_width(logo, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(logo, 0, LV_PART_MAIN);
        lv_obj_add_flag(logo, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(logo, sh_home_event, LV_EVENT_CLICKED, nullptr);
        lv_obj_remove_flag(logo, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(logo, LV_ALIGN_LEFT_MID, 10, 0);
        lv_obj_t *wim = lv_image_create(logo);
        lv_image_set_src(wim, &wolf);
        lv_obj_set_size(wim, side_btn, side_btn);
        lv_image_set_inner_align(wim, LV_IMAGE_ALIGN_CONTAIN);
        lv_obj_center(wim);
    } else {
        lv_obj_t *back = lv_button_create(s_bar);
        lv_obj_set_size(back, side_btn, side_btn);
        lv_obj_align(back, LV_ALIGN_LEFT_MID, 10, 0);
        shell_style_topbar_btn(back);
        lv_obj_t *bl = lv_label_create(back);
        /* Top-left icon becomes “Back to Home” when inside an app */
        lv_label_set_text(bl, LV_SYMBOL_HOME);
        lv_obj_set_style_text_font(bl, APP_FONT_HEADING, LV_PART_MAIN);
        lv_obj_set_style_text_color(bl, lv_color_hex(APP_C_TEXT), LV_PART_MAIN);
        lv_obj_center(bl);
        /* 2× visual size (default scale = 256) */
        lv_obj_set_style_transform_scale(bl, 512, LV_PART_MAIN);
        lv_obj_add_event_cb(bl, home_icon_size_cb, LV_EVENT_SIZE_CHANGED, nullptr);
        lv_obj_update_layout(back);
        home_icon_center_pivot(bl);
        lv_obj_add_event_cb(back, sh_backhome_event, LV_EVENT_CLICKED, nullptr);
    }

    /* Right cluster: optional action + battery (no clock, no Wi‑Fi) */
    lv_obj_t *rr = lv_obj_create(s_bar);
    lv_obj_set_size(rr, LV_SIZE_CONTENT, side_btn);
    lv_obj_set_style_bg_opa(rr, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(rr, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(rr, 0, LV_PART_MAIN);
    lv_obj_set_layout(rr, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(rr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rr, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(rr, 8, LV_PART_MAIN);
    lv_obj_remove_flag(rr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(rr, LV_ALIGN_RIGHT_MID, -10, 0);

    if (right_caption && on_right) {
        lv_obj_t *br = lv_button_create(rr);
        lv_obj_set_size(br, LV_SIZE_CONTENT, side_btn);
        lv_obj_set_style_pad_hor(br, 14, LV_PART_MAIN);
        shell_style_topbar_btn(br);
        lv_obj_t *lr = lv_label_create(br);
        lv_label_set_text(lr, right_caption);
        lv_obj_set_style_text_font(lr, APP_FONT_SUB, LV_PART_MAIN);
        lv_obj_set_style_text_color(lr, lv_color_hex(APP_C_TEXT), LV_PART_MAIN);
        lv_obj_center(lr);
        lv_obj_add_event_cb(br, sh_right_event, LV_EVENT_CLICKED, (void *)on_right);
    }

    lv_obj_t *brow = lv_obj_create(rr);
    lv_obj_set_size(brow, LV_SIZE_CONTENT, side_btn);
    lv_obj_set_style_bg_opa(brow, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(brow, 0, LV_PART_MAIN);
    lv_obj_set_layout(brow, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(brow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(brow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(brow, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(brow, 0, LV_PART_MAIN);
    lv_obj_remove_flag(brow, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *batt_chrg = lv_label_create(brow);
    lv_label_set_text(batt_chrg, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_font(batt_chrg, APP_FONT_HEADING, LV_PART_MAIN);
    /* Add a small gap between charge and battery glyph */
    lv_obj_set_style_pad_right(batt_chrg, 6, LV_PART_MAIN);
    lv_obj_add_flag(batt_chrg, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *batt_sym = lv_label_create(brow);
    lv_obj_set_style_text_font(batt_sym, APP_FONT_HEADING, LV_PART_MAIN);
    lv_obj_t *batt_num = lv_label_create(brow);
    lv_obj_set_style_text_font(batt_num, APP_FONT_HEADING, LV_PART_MAIN);
    s_shell_batt.chrg = batt_chrg;
    s_shell_batt.sym  = batt_sym;
    s_shell_batt.num  = batt_num;
    shell_batt_refresh_all();

    lv_obj_t *pkb = lv_button_create(rr);
    lv_obj_set_size(pkb, LV_SIZE_CONTENT, side_btn);
    lv_obj_set_style_pad_hor(pkb, 12, LV_PART_MAIN);
    shell_style_topbar_btn(pkb);
    lv_obj_t *pl = lv_label_create(pkb);
    lv_label_set_text(pl, LV_SYMBOL_POWER);
    lv_obj_set_style_text_font(pl, APP_FONT_HEADING, LV_PART_MAIN);
    lv_obj_set_style_text_color(pl, lv_color_hex(APP_C_TEXT), LV_PART_MAIN);
    lv_obj_center(pl);
    lv_obj_add_event_cb(pkb, sh_power_menu_event, LV_EVENT_CLICKED, nullptr);

    /* True horizontal center of bar (drawn after side widgets so label stays readable) */
    lv_obj_t *tlab = lv_label_create(s_bar);
    lv_label_set_text(tlab, APP_NAME);
    lv_obj_set_style_text_font(tlab, APP_FONT_HEADING, LV_PART_MAIN);
    lv_obj_set_style_text_color(tlab, lv_color_hex(APP_C_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_align(tlab, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(tlab, LV_ALIGN_CENTER, 0, 0);
    s_shell_batt_timer = lv_timer_create(shell_batt_timer_cb, 1200, nullptr);
    lv_timer_set_repeat_count(s_shell_batt_timer, -1);

    s_cbody = lv_obj_create(scr);
    lv_obj_set_width(s_cbody, APP_LCD_WIDTH);
    lv_obj_set_flex_grow(s_cbody, 1);
    lv_obj_set_style_margin_top(s_cbody, 0, LV_PART_MAIN);
    /* Unified app background: match Home (white) */
    color_bg(s_cbody, APP_C_HOME_SCREEN);
    lv_obj_set_style_pad_all(s_cbody, (lv_coord_t)APP_SAFE_INSET_V, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_cbody, 8, LV_PART_MAIN);
    lv_obj_set_style_radius(s_cbody, 0, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(s_cbody, false, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_cbody, (lv_coord_t)APP_SAFE_INSET_V, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_cbody, 8, LV_PART_MAIN);
    lv_obj_set_layout(s_cbody, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(s_cbody, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(s_cbody, LV_OBJ_FLAG_SCROLLABLE);

    /* Treat any press anywhere as activity (wakes dimmed backlight). */
    lv_obj_add_event_cb(scr, [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_PRESSED) user_activity_poke();
    }, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(s_cbody, [](lv_event_t *e) {
        if (lv_event_get_code(e) == LV_EVENT_PRESSED) user_activity_poke();
    }, LV_EVENT_PRESSED, nullptr);

    if (!s_idle_timer) {
        s_last_activity_ms = (uint32_t)lv_tick_get();
        s_idle_timer       = lv_timer_create(idle_timer_cb, 250, nullptr);
        lv_timer_set_repeat_count(s_idle_timer, -1);
    }
}

static lv_obj_t *content_ptr() { return s_cbody; }

static void app_style_ios_slider(lv_obj_t *sl) {
    lv_obj_set_style_bg_color(sl, lv_color_hex(0xE5E5EA), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(sl, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sl, lv_color_hex(APP_C_ICON_ACCENT_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(sl, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_border_color(sl, lv_color_hex(0xD1D1D6), LV_PART_KNOB);
    lv_obj_set_style_border_width(sl, 1, LV_PART_KNOB);
}

static lv_obj_t *app_screen_title(lv_obj_t *c, const char *title) {
    lv_obj_t *t = lv_label_create(c);
    lv_label_set_text(t, title);
    /* Settings-style titles: bigger + centered */
    lv_obj_set_width(t, lv_pct(100));
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(t, APP_FONT_HERO, LV_PART_MAIN);
    lv_obj_set_style_text_color(t, lv_color_hex(APP_C_SHEET_TEXT), LV_PART_MAIN);
    return t;
}

static void app_style_muted_card(lv_obj_t *o) {
    color_bg(o, 0xF2F2F7);
    lv_obj_set_style_radius(o, 16, LV_PART_MAIN);
    lv_obj_set_style_border_color(o, lv_color_hex(0xE5E5EA), LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(o, 16, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(o, 0, LV_PART_MAIN);
}

/* ---------- Home: iPhone-like grid (4 cols, vertical scroll) ---------- */
enum { HOME_ICON_COLS = 4, HOME_ICON_GAP = 22, HOME_ICON_PAD = 26 };

static const struct {
    const char *sym;
    const char *label;
    AppScreen   id;
} k_apps[] = {
    { LV_SYMBOL_SETTINGS, "Settings", AppScreen::Settings },
    { LV_SYMBOL_DIRECTORY, "Files", AppScreen::Files },
    { LV_SYMBOL_WIFI, "Wi‑Fi", AppScreen::WiFi },
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

    color_bg(c, APP_C_HOME_SCREEN);
    lv_obj_set_style_radius(c, 0, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(c, false, LV_PART_MAIN);
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
    const int cell      = (content_w - HOME_ICON_GAP * (HOME_ICON_COLS - 1)) / HOME_ICON_COLS;
    const int cell_h   = cell + 34; /* icon box + label */

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
        lv_obj_set_size(icon_box, (lv_coord_t)cell, (lv_coord_t)cell);
        color_bg(icon_box, APP_C_ICON_ACCENT_BLUE);
        lv_obj_set_style_radius(icon_box, APP_RADIUS_TILE, LV_PART_MAIN);
        lv_obj_set_style_border_width(icon_box, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(icon_box, 6, LV_PART_MAIN);
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
        lv_obj_set_style_transform_scale_x(ic, 372, LV_PART_MAIN);
        lv_obj_set_style_transform_scale_y(ic, 372, LV_PART_MAIN);

        lv_obj_t *cap = lv_label_create(hit);
        lv_label_set_text(cap, k_apps[i].label);
        lv_obj_set_style_text_font(cap, APP_FONT_HEADING, LV_PART_MAIN);
        lv_obj_set_style_text_color(cap, lv_color_hex(APP_C_SHEET_TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_letter_space(cap, 1, LV_PART_MAIN);
        lv_label_set_long_mode(cap, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_width(cap, (lv_coord_t)(cell - 4));
        lv_obj_add_flag(cap, LV_OBJ_FLAG_EVENT_BUBBLE);
    }
}

/* placeholder pages */
static void noop_right(lv_event_t *e) { (void)e; }

static void files_action_sheet_cb(lv_event_t *e) {
    (void)e;
    LvglHal::click_feedback();
    lv_obj_t *mb = lv_msgbox_create(lv_layer_top());
    lv_msgbox_add_title(mb, "File action");
    lv_msgbox_add_text(mb, "Open / share / delete — connect FS + actions here.");
    lv_msgbox_add_footer_button(mb, "Open");
    lv_msgbox_add_footer_button(mb, "Delete");
    lv_msgbox_add_close_button(mb);
    lv_obj_center(mb);
}

void view_files(void *user_ctx) {
    (void)user_ctx;
    shell_mount("Files", files_action_sheet_cb, "···", false);
    lv_obj_t *c = content_ptr();
    app_screen_title(c, "Files");

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
    color_bg(list, APP_C_HOME_SCREEN);
    lv_obj_set_style_radius(list, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_all(list, 4, LV_PART_MAIN);
    lv_obj_set_style_border_color(list, lv_color_hex(0xE5E5EA), LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 1, LV_PART_MAIN);
    const char *items[] = { "docs/", "photo.jpg", "notes.txt", "archive.zip" };
    for (unsigned i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
        lv_list_add_button(list, nullptr, items[i]);
    }
}

void view_wifi() {
    shell_mount("WiFi", noop_right, "Refresh", false);
    lv_obj_t *c    = content_ptr();
    app_screen_title(c, "Wi‑Fi");
    lv_obj_t *list = lv_list_create(c);
    lv_obj_set_size(list, lv_pct(100), (lv_coord_t)APP_CONTENT_H - 80);
    apply_scroll_tabled(list);
    color_bg(list, APP_C_HOME_SCREEN);
    lv_obj_set_style_radius(list, 14, LV_PART_MAIN);
    lv_obj_set_style_border_color(list, lv_color_hex(0xE5E5EA), LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 1, LV_PART_MAIN);
    const char *aps[] = { "· Arcane-AP  [good]", "· GuestNet  [ok]", "· Lab-5G  [weak]" };
    for (unsigned i = 0; i < sizeof(aps) / sizeof(aps[0]); ++i) {
        lv_list_add_button(list, nullptr, aps[i]);
    }
    lv_obj_t *hint = lv_label_create(c);
    lv_label_set_text(hint, "Connect flow + on-screen keyboard: hook WiFi API here.");
    lv_obj_set_style_text_color(hint, lv_color_hex(APP_C_ICON_GREY_MID), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, APP_FONT_CAP, LV_PART_MAIN);
}

void view_camera() {
    shell_mount("Camera", nullptr, nullptr, false);
    lv_obj_t *c = content_ptr();
    app_screen_title(c, "Camera");
    lv_obj_t *box = lv_obj_create(c);
    lv_obj_set_width(box, lv_pct(100));
    app_style_muted_card(box);
    lv_obj_t *t = lv_label_create(box);
    lv_label_set_text(t, "Camera: implement with M5/CSI stack for this board.");
    lv_obj_set_style_text_font(t, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(t, lv_color_hex(APP_C_ICON_GREY_DARK), LV_PART_MAIN);
    lv_obj_set_width(t, lv_pct(100));
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
}
void view_imu() {
    shell_mount("IMU / Sensors", nullptr, nullptr, false);
    lv_obj_t *c = content_ptr();
    app_screen_title(c, "Sensors");
    lv_obj_t *box = lv_obj_create(c);
    lv_obj_set_width(box, lv_pct(100));
    app_style_muted_card(box);
    lv_obj_t *t = lv_label_create(box);
    lv_label_set_text(t, "IMU: map your Tab5 I2C bus and chip in a driver, then show live data here.");
    lv_obj_set_style_text_color(t, lv_color_hex(APP_C_ICON_GREY_DARK), LV_PART_MAIN);
    lv_obj_set_style_text_font(t, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_width(t, lv_pct(100));
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
}
void view_power() {
    shell_mount("Power", noop_right, "Info", false);
    lv_obj_t *c = content_ptr();
    app_screen_title(c, "Power");
    lv_obj_t *box = lv_obj_create(c);
    lv_obj_set_width(box, lv_pct(100));
    app_style_muted_card(box);
    lv_obj_t *t = lv_label_create(box);
    {
        M5.update();
        int pct = (int)M5.Power.getBatteryLevel();
        int mV  = (int)M5.Power.getBatteryVoltage();
        char b[80];
        snprintf(b, sizeof(b), "Battery: %d%%  ·  %d mV\n(Sleep policies are board-specific; extend Power_Class usage.)", pct, mV);
        lv_label_set_text(t, b);
    }
    lv_obj_set_style_text_color(t, lv_color_hex(APP_C_ICON_GREY_DARK), LV_PART_MAIN);
    lv_obj_set_style_text_font(t, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_width(t, lv_pct(100));
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
}

void view_power_menu() {
    shell_mount("Power menu", nullptr, nullptr, false);
    lv_obj_t *c = content_ptr();
    lv_obj_t *wrap = lv_obj_create(c);
    lv_obj_set_width(wrap, lv_pct(100));
    lv_obj_set_flex_grow(wrap, 1);
    color_bg(wrap, APP_C_HOME_SCREEN);
    lv_obj_set_style_border_width(wrap, 0, LV_PART_MAIN);
    lv_obj_set_layout(wrap, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(wrap, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(wrap, 24, LV_PART_MAIN);
    lv_obj_remove_flag(wrap, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn = lv_button_create(wrap);
    lv_obj_set_size(btn, 220, 200);
    color_bg(btn, 0xF2F2F7);
    lv_obj_set_style_radius(btn, 20, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(0xD0D7DE), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(btn, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 16, LV_PART_MAIN);

    lv_obj_t *ic = lv_label_create(btn);
    lv_label_set_text(ic, LV_SYMBOL_POWER);
    lv_obj_set_style_text_font(ic, APP_FONT_HERO, LV_PART_MAIN);
    lv_obj_set_style_text_color(ic, lv_color_hex(APP_C_ICON_GREY_DARK), LV_PART_MAIN);

    lv_obj_t *cap = lv_label_create(btn);
    lv_label_set_text(cap, "Power Off");
    lv_obj_set_style_text_font(cap, APP_FONT_SUB, LV_PART_MAIN);
    lv_obj_set_style_text_color(cap, lv_color_hex(APP_C_ICON_GREY_DARK), LV_PART_MAIN);

    lv_obj_add_event_cb(btn, power_off_do_cb, LV_EVENT_CLICKED, nullptr);
}

void view_sd() {
    shell_mount("SD / Storage", nullptr, nullptr, false);
    lv_obj_t *c = content_ptr();
    app_screen_title(c, "Storage");
    lv_obj_t *box = lv_obj_create(c);
    lv_obj_set_width(box, lv_pct(100));
    app_style_muted_card(box);
    lv_obj_t *t = lv_label_create(box);
    lv_label_set_text(t, "Mount SD, show volume and low-level tools. Uses same Files list UX when browsing.");
    lv_obj_set_style_text_color(t, lv_color_hex(APP_C_ICON_GREY_DARK), LV_PART_MAIN);
    lv_obj_set_style_text_font(t, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_width(t, lv_pct(100));
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
}
static void touchtest_press_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_PRESSED)
        return;
    lv_indev_t *d = lv_indev_get_act();
    lv_point_t  p;
    if (d)
        lv_indev_get_point(d, &p);
    else
        p.x = p.y = 0;
    char b[64];
    lv_obj_t *lab = (lv_obj_t *)lv_event_get_user_data(e);
    snprintf(b, sizeof(b), "x=%d  y=%d  (post-cal indev)", (int)p.x, (int)p.y);
    if (lab) lv_label_set_text(lab, b);
}

void view_touchtest() {
    shell_mount("Touch test", nullptr, nullptr, false);
    lv_obj_t *c   = content_ptr();
    app_screen_title(c, "Touch test");
    lv_obj_t *lab = lv_label_create(c);
    lv_label_set_text(lab, "—");
    lv_obj_set_style_text_font(lab, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(lab, lv_color_hex(APP_C_ICON_ACCENT_BLUE), LV_PART_MAIN);
    lv_obj_t *z = lv_obj_create(c);
    lv_obj_set_size(z, lv_pct(100), (lv_coord_t)APP_CONTENT_H - 100);
    app_style_muted_card(z);
    apply_scroll_tabled(z);
    lv_obj_add_event_cb(z, touchtest_press_cb, LV_EVENT_PRESSED, lab);
}
void view_i2c() {
    shell_mount("I2C scan", noop_right, "Scan", false);
    lv_obj_t *c = content_ptr();
    app_screen_title(c, "I2C");
    lv_obj_t *box = lv_obj_create(c);
    lv_obj_set_width(box, lv_pct(100));
    app_style_muted_card(box);
    lv_obj_t *t = lv_label_create(box);
    lv_label_set_text(t, "Wire/ESP-IDF: scan primary I2C bus; print addresses in a list.");
    lv_obj_set_style_text_color(t, lv_color_hex(APP_C_ICON_GREY_DARK), LV_PART_MAIN);
    lv_obj_set_style_text_font(t, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_width(t, lv_pct(100));
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
}
void view_rtc() {
    shell_mount("Clock / RTC", nullptr, nullptr, false);
    lv_obj_t *c = content_ptr();
    app_screen_title(c, "Date & time");
    lv_obj_t *box = lv_obj_create(c);
    lv_obj_set_width(box, lv_pct(100));
    app_style_muted_card(box);
    lv_obj_t *t = lv_label_create(box);
    lv_label_set_text(t, "RTC: bind RX8130/RV3028 (whatever Tab5 has) and show UTC/local.");
    lv_obj_set_style_text_color(t, lv_color_hex(APP_C_ICON_GREY_DARK), LV_PART_MAIN);
    lv_obj_set_style_text_font(t, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_width(t, lv_pct(100));
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
}

static void settings_reset_cal(lv_event_t *e) {
    (void)e;
    LvglHal::click_feedback();
    TouchCalStore::clear();
    esp_restart();
}

enum class SettingsCat : uint8_t { Power = 0, Comms = 1, Security = 2, Sensors = 3 };

static SettingsCat s_settings_cat = SettingsCat::Power;
static int         s_cpu_mhz       = 240;
static int         s_lora_mix      = 50;  /* 0..100; UI-only */
static bool        s_mesh_gateway  = false; /* UI-only */
static int         s_bl_timeout_s  = 0;   /* 0,5,10,30 */
static int         s_dim_timeout_s = 0;   /* 0,5,10,30 screen dim */
static uint8_t     s_volume_pct    = 70;  /* 0..100 → M5.Speaker */

static bool       s_settings_dirty       = false;
static lv_obj_t * s_settings_save_top    = nullptr;
static lv_obj_t * s_settings_save_bottom = nullptr;

void arc_settings_load_from_nvs() {
    Preferences p;
    if (!p.begin(ARC_SETTINGS_NVS, true))
        return;
    if (p.getUInt("magic", 0) != ARC_SETTINGS_MAGIC) {
        p.end();
        return;
    }
    const uint32_t bl = p.getUInt("bl_s", 0);
    if (bl == 0u || bl == 5u || bl == 10u || bl == 30u) {
        s_bl_timeout_s         = (int)bl;
        s_backlight_timeout_ms = bl * 1000u;
    }
    {
        const uint32_t dim = p.getUInt("dim_s", 255u);
        if (dim == 0u || dim == 5u || dim == 10u || dim == 30u) {
            s_dim_timeout_s  = (int)dim;
            s_dim_timeout_ms = dim * 1000u;
        }
    }
    const int cpu = (int)p.getInt("cpu", 240);
    if (cpu == 80 || cpu == 160 || cpu == 240) {
        s_cpu_mhz = cpu;
        setCpuFrequencyMhz((uint32_t)cpu);
    }
    int mix = (int)p.getInt("lora", 50);
    if (mix < 0)
        mix = 0;
    if (mix > 100)
        mix = 100;
    s_lora_mix     = mix;
    s_mesh_gateway = p.getBool("mesh", false);

    const uint32_t bri = p.getUInt("bri", 0);
    if (bri >= 1u && bri <= 100u) {
        s_backlight_restore = (uint8_t)bri;
        M5.Display.setBrightness((uint8_t)((255u * bri) / 100u));
    }
    const uint32_t vol_d = p.getUInt("vol", 101u);
    if (vol_d <= 100u) {
        s_volume_pct = (uint8_t)vol_d;
        M5.Speaker.setVolume((uint8_t)((255u * vol_d) / 100u));
    }
    p.end();
}

static void settings_style_save_btn(lv_obj_t *btn) {
    lv_obj_set_style_bg_color(btn, lv_color_hex(APP_C_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(btn, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(btn, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN);
}

static void settings_persist_to_nvs() {
    Preferences p;
    if (!p.begin(ARC_SETTINGS_NVS, false))
        return;
    p.putUInt("magic", ARC_SETTINGS_MAGIC);
    p.putUInt("bl_s", (uint32_t)s_bl_timeout_s);
    p.putUInt("dim_s", (uint32_t)s_dim_timeout_s);
    p.putInt("cpu", s_cpu_mhz);
    p.putInt("lora", s_lora_mix);
    p.putBool("mesh", s_mesh_gateway);
    p.putUInt("bri", (uint32_t)s_backlight_restore);
    p.putUInt("vol", (uint32_t)s_volume_pct);
    p.end();
}

static void settings_on_saved() {
    s_settings_dirty = false;
    if (s_settings_save_top && lv_obj_is_valid(s_settings_save_top))
        lv_obj_add_flag(s_settings_save_top, LV_OBJ_FLAG_HIDDEN);
    if (s_settings_save_bottom && lv_obj_is_valid(s_settings_save_bottom))
        lv_obj_add_flag(s_settings_save_bottom, LV_OBJ_FLAG_HIDDEN);
}

static void settings_save_click_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
        return;
    LvglHal::click_feedback();
    settings_persist_to_nvs();
    settings_on_saved();
}

static void settings_mark_dirty() {
    s_settings_dirty = true;
    if (s_settings_save_top && lv_obj_is_valid(s_settings_save_top))
        lv_obj_remove_flag(s_settings_save_top, LV_OBJ_FLAG_HIDDEN);
    if (s_settings_save_bottom && lv_obj_is_valid(s_settings_save_bottom))
        lv_obj_remove_flag(s_settings_save_bottom, LV_OBJ_FLAG_HIDDEN);
}

static void settings_cpu_refresh_chips(lv_obj_t *row) {
    const uint32_t n = lv_obj_get_child_count(row);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t *b = lv_obj_get_child(row, i);
        if (!b)
            continue;
        const intptr_t mhz = (intptr_t)lv_obj_get_user_data(b);
        if (mhz != 80 && mhz != 160 && mhz != 240)
            continue;
        const bool on = ((int)mhz == s_cpu_mhz);
        lv_obj_set_style_bg_color(b, lv_color_hex(on ? APP_C_ACCENT : 0xE8E8ED), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_t *lb = lv_obj_get_child(b, 0);
        if (lb)
            lv_obj_set_style_text_color(lb, lv_color_hex(on ? 0xFFFFFF : APP_C_SHEET_TEXT), LV_PART_MAIN);
    }
}

static void settings_cpu_chip_clicked(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
        return;
    LvglHal::click_feedback();
    lv_obj_t *b = (lv_obj_t *)lv_event_get_target(e);
    const intptr_t mhz = (intptr_t)lv_obj_get_user_data(b);
    if (mhz != 80 && mhz != 160 && mhz != 240)
        return;
    s_cpu_mhz = (int)mhz;
    setCpuFrequencyMhz((uint32_t)s_cpu_mhz);
    settings_cpu_refresh_chips(lv_obj_get_parent(b));
    settings_mark_dirty();
}

static void settings_add_cpu_mhz_row(lv_obj_t *parent) {
    lv_obj_t *hdr = lv_label_create(parent);
    lv_label_set_text(hdr, "CPU frequency");
    lv_obj_set_style_text_font(hdr, APP_FONT_SUB, LV_PART_MAIN);
    lv_obj_set_style_text_color(hdr, lv_color_hex(APP_C_SHEET_TEXT), LV_PART_MAIN);

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 8, LV_PART_MAIN);

    const int freqs[] = {80, 160, 240};
    for (unsigned fi = 0; fi < 3; ++fi) {
        const int mhz = freqs[fi];
        lv_obj_t *b = lv_button_create(row);
        lv_obj_set_flex_grow(b, 1);
        lv_obj_set_height(b, 44);
        lv_obj_set_style_radius(b, 12, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(b, 0, LV_PART_MAIN);
        lv_obj_set_user_data(b, (void *)(intptr_t)mhz);
        lv_obj_t *lb = lv_label_create(b);
        char buf[24];
        snprintf(buf, sizeof(buf), "%d MHz", mhz);
        lv_label_set_text(lb, buf);
        lv_obj_set_style_text_font(lb, APP_FONT_BODY, LV_PART_MAIN);
        lv_obj_center(lb);
        lv_obj_add_event_cb(b, settings_cpu_chip_clicked, LV_EVENT_CLICKED, nullptr);
    }
    settings_cpu_refresh_chips(row);
}

static void settings_install_save_footer(lv_obj_t *panel) {
    lv_obj_t *row = lv_obj_create(panel);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_top(row, 28, LV_PART_MAIN);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *b = lv_button_create(row);
    settings_style_save_btn(b);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, "Save changes");
    lv_obj_set_style_text_font(l, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, settings_save_click_cb, LV_EVENT_CLICKED, nullptr);
    if (!s_settings_dirty)
        lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
    s_settings_save_bottom = b;
}

static void settings_sidebar_btn_style(lv_obj_t *b, bool active) {
    lv_obj_set_style_radius(b, 18, LV_PART_MAIN);
    lv_obj_set_style_border_width(b, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_hex(active ? 0xE8F1FB : 0xF2F2F7), LV_PART_MAIN);
}

static void settings_content_title(lv_obj_t *parent, const char *t) {
    lv_obj_t *lab = lv_label_create(parent);
    lv_label_set_text(lab, t);
    lv_obj_set_width(lab, lv_pct(100));
    lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(lab, APP_FONT_HEADING, LV_PART_MAIN);
    lv_obj_set_style_text_color(lab, lv_color_hex(APP_C_SHEET_TEXT), LV_PART_MAIN);
}

static void settings_add_dropdown_row(lv_obj_t *parent, const char *label, const char *opts, int sel,
                                     void (*on_change)(int)) {
    /* One block per setting: title row, then full-width dropdown (avoids overlap). */
    lv_obj_t *wrap = lv_obj_create(parent);
    lv_obj_set_width(wrap, lv_pct(100));
    lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(wrap, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(wrap, 0, LV_PART_MAIN);
    lv_obj_set_layout(wrap, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(wrap, 8, LV_PART_MAIN);
    lv_obj_remove_flag(wrap, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(wrap, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *l = lv_label_create(wrap);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_font(l, APP_FONT_SUB, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(APP_C_SHEET_TEXT), LV_PART_MAIN);
    lv_obj_set_width(l, lv_pct(100));

    lv_obj_t *dd = lv_dropdown_create(wrap);
    lv_obj_set_width(dd, lv_pct(100));
    lv_dropdown_set_options(dd, opts);
    lv_dropdown_set_selected(dd, (uint16_t)sel);
    lv_obj_set_style_text_font(dd, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_remove_flag(dd, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(dd, LV_SCROLLBAR_MODE_OFF);

    lv_obj_add_event_cb(
        dd,
        [](lv_event_t *e) {
            if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
            auto fn = (void (*)(int))(uintptr_t)lv_event_get_user_data(e);
            int  v  = (int)lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(e));
            if (fn) fn(v);
        },
        LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)on_change);
}

struct SettingsSliderUd {
    void (*fn)(int);
    lv_obj_t *val_lbl;
    const char *fmt;
};

static void settings_slider_ud_free_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_DELETE)
        return;
    lv_obj_t *sl = (lv_obj_t *)lv_event_get_target(e);
    SettingsSliderUd *ud = (SettingsSliderUd *)lv_obj_get_user_data(sl);
    if (ud) {
        lv_free(ud);
        lv_obj_set_user_data(sl, nullptr);
    }
}

static void settings_slider_evt_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED)
        return;
    lv_obj_t *sl = (lv_obj_t *)lv_event_get_target(e);
    SettingsSliderUd *ud = (SettingsSliderUd *)lv_obj_get_user_data(sl);
    if (!ud)
        return;
    const int nv = (int)lv_slider_get_value(sl);
    char b[32];
    snprintf(b, sizeof(b), ud->fmt ? ud->fmt : "%d", nv);
    if (ud->val_lbl)
        lv_label_set_text(ud->val_lbl, b);
    if (ud->fn)
        ud->fn(nv);
}

static void settings_add_switch_row(lv_obj_t *parent, const char *label, bool initial, void (*on_toggle)(bool)) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *l = lv_label_create(row);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_font(l, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(APP_C_SHEET_TEXT), LV_PART_MAIN);

    lv_obj_t *sw = lv_switch_create(row);
    if (initial) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(
        sw,
        [](lv_event_t *e) {
            if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
            auto fn = (void (*)(bool))(uintptr_t)lv_event_get_user_data(e);
            bool v  = lv_obj_has_state((lv_obj_t *)lv_event_get_target(e), LV_STATE_CHECKED);
            if (fn) fn(v);
        },
        LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)on_toggle);
}

static void settings_add_slider_row(lv_obj_t *parent, const char *label, int minv, int maxv, int v,
                                   void (*on_set)(int), const char *val_fmt) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_width(box, lv_pct(100));
    app_style_muted_card(box);
    lv_obj_set_style_pad_all(box, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_row(box, 10, LV_PART_MAIN);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *top = lv_obj_create(box);
    lv_obj_set_width(top, lv_pct(100));
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(top, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(top, 0, LV_PART_MAIN);
    lv_obj_remove_flag(top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(top, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_layout(top, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *l = lv_label_create(top);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_font(l, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(APP_C_SHEET_TEXT), LV_PART_MAIN);

    lv_obj_t *val = lv_label_create(top);
    lv_obj_set_style_text_font(val, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(val, lv_color_hex(APP_C_SHEET_TEXT_MUTE), LV_PART_MAIN);

    lv_obj_t *sl = lv_slider_create(box);
    lv_obj_set_width(sl, lv_pct(100));
    lv_slider_set_range(sl, minv, maxv);
    lv_slider_set_value(sl, v, LV_ANIM_OFF);
    app_style_ios_slider(sl);
    lv_obj_remove_flag(sl, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(sl, LV_SCROLLBAR_MODE_OFF);

    const char *fmt = val_fmt ? val_fmt : "%d";
    char initb[32];
    snprintf(initb, sizeof(initb), fmt, v);
    lv_label_set_text(val, initb);

    SettingsSliderUd *ud = (SettingsSliderUd *)lv_malloc(sizeof(SettingsSliderUd));
    if (ud) {
        ud->fn      = on_set;
        ud->val_lbl = val;
        ud->fmt     = fmt;
        lv_obj_set_user_data(sl, ud);
        lv_obj_add_event_cb(sl, settings_slider_evt_cb, LV_EVENT_VALUE_CHANGED, nullptr);
        lv_obj_add_event_cb(sl, settings_slider_ud_free_cb, LV_EVENT_DELETE, nullptr);
    }
}

static void rtc_sync_msg(const char *method) {
    lv_obj_t *mb = lv_msgbox_create(lv_layer_top());
    lv_msgbox_add_title(mb, "RTC time sync");
    lv_msgbox_add_text(mb, method);
    lv_msgbox_add_footer_button(mb, "OK");
    lv_obj_center(mb);
}

static void settings_build_content(lv_obj_t *panel) {
    lv_obj_clean(panel);
    s_settings_save_bottom = nullptr;
    lv_obj_set_style_pad_row(panel, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 12, LV_PART_MAIN);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    /* Make right panel scrollable for longer lists. */
    lv_obj_add_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);
    apply_scroll_tabled(panel);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    if (s_settings_cat == SettingsCat::Power) {
        settings_content_title(panel, "Power");

        settings_add_slider_row(panel, "Display brightness", 5, 100, (int)s_backlight_restore,
                                [](int pct) {
                                    s_backlight_restore = (uint8_t)pct;
                                    M5.Display.setBrightness((uint8_t)((255u * (uint32_t)pct) / 100u));
                                    user_activity_poke();
                                    settings_mark_dirty();
                                },
                                "%d%%");

        settings_add_slider_row(panel, "Volume", 0, 100, (int)s_volume_pct,
                                [](int pct) {
                                    s_volume_pct = (uint8_t)pct;
                                    M5.Speaker.setVolume((uint8_t)((255u * (uint32_t)pct) / 100u));
                                    settings_mark_dirty();
                                },
                                "%d%%");

        settings_add_dropdown_row(panel, "Backlight timeout",
                                  "Off\n5 seconds\n10 seconds\n30 seconds",
                                  (s_bl_timeout_s == 0) ? 0 : (s_bl_timeout_s == 5) ? 1 : (s_bl_timeout_s == 10) ? 2 : 3,
                                  [](int sel) {
                                      s_bl_timeout_s = (sel == 0) ? 0 : (sel == 1) ? 5 : (sel == 2) ? 10 : 30;
                                      s_backlight_timeout_ms = (uint32_t)s_bl_timeout_s * 1000u;
                                      user_activity_poke();
                                      settings_mark_dirty();
                                  });

        settings_add_dropdown_row(panel, "Screen dim timeout",
                                  "Off\n5 seconds\n10 seconds\n30 seconds",
                                  (s_dim_timeout_s == 0) ? 0 : (s_dim_timeout_s == 5) ? 1 : (s_dim_timeout_s == 10) ? 2 : 3,
                                  [](int sel) {
                                      s_dim_timeout_s = (sel == 0) ? 0 : (sel == 1) ? 5 : (sel == 2) ? 10 : 30;
                                      s_dim_timeout_ms  = (uint32_t)s_dim_timeout_s * 1000u;
                                      user_activity_poke();
                                      settings_mark_dirty();
                                  });

        settings_add_cpu_mhz_row(panel);
    } else if (s_settings_cat == SettingsCat::Comms) {
        settings_content_title(panel, "Comms");

        settings_add_slider_row(panel, "LoRa spreading factor", 0, 100, s_lora_mix,
                                [](int v) {
                                    s_lora_mix = v;
                                    settings_mark_dirty();
                                },
                                "%d");
        lv_obj_t *hint = lv_label_create(panel);
        lv_label_set_text(hint, "Left: High range / low speed   ·   Right: Low range / high speed");
        lv_obj_set_style_text_font(hint, APP_FONT_CAP, LV_PART_MAIN);
        lv_obj_set_style_text_color(hint, lv_color_hex(APP_C_SHEET_TEXT_MUTE), LV_PART_MAIN);

        settings_add_switch_row(panel, "Mesh gateway (Repeater)", s_mesh_gateway, [](bool on) {
            s_mesh_gateway = on;
            settings_mark_dirty();
        });
    } else if (s_settings_cat == SettingsCat::Security) {
        settings_content_title(panel, "Security");
        lv_obj_t *box = lv_obj_create(panel);
        lv_obj_set_width(box, lv_pct(100));
        app_style_muted_card(box);
        lv_obj_t *t = lv_label_create(box);
        lv_label_set_text(t, "Security settings placeholder.");
        lv_obj_set_style_text_font(t, APP_FONT_BODY, LV_PART_MAIN);
        lv_obj_set_style_text_color(t, lv_color_hex(APP_C_SHEET_TEXT), LV_PART_MAIN);
    } else { /* Sensors */
        settings_content_title(panel, "Sensors");

        lv_obj_t *box = lv_obj_create(panel);
        lv_obj_set_width(box, lv_pct(100));
        app_style_muted_card(box);

        lv_obj_t *t = lv_label_create(box);
        lv_label_set_text(t, "RTC time sync");
        lv_obj_set_style_text_font(t, APP_FONT_BODY, LV_PART_MAIN);
        lv_obj_set_style_text_color(t, lv_color_hex(APP_C_SHEET_TEXT), LV_PART_MAIN);

        lv_obj_t *row = lv_obj_create(panel);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
        lv_obj_set_layout(row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 10, LV_PART_MAIN);

        auto mk = [row](const char *cap, const char *msg) {
            lv_obj_t *b = lv_button_create(row);
            lv_obj_set_style_radius(b, 18, LV_PART_MAIN);
            lv_obj_set_style_pad_hor(b, 14, LV_PART_MAIN);
            lv_obj_set_style_pad_ver(b, 10, LV_PART_MAIN);
            lv_obj_t *l = lv_label_create(b);
            lv_label_set_text(l, cap);
            lv_obj_set_style_text_font(l, APP_FONT_BODY, LV_PART_MAIN);
            lv_obj_center(l);
            lv_obj_add_event_cb(
                b,
                [](lv_event_t *e) {
                    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                    rtc_sync_msg((const char *)lv_event_get_user_data(e));
                },
                LV_EVENT_CLICKED, (void *)msg);
        };
        mk("Manual", "Manual: set RTC date/time.");
        mk("GPS", "GPS: sync RTC from a connected GPS unit (TODO).");
        mk("Internet", "Internet: sync RTC via NTP when Wi‑Fi is connected (TODO).");
    }

    settings_install_save_footer(panel);
}

void view_settings() {
    shell_mount("Settings", nullptr, nullptr, false);
    s_settings_save_top    = nullptr;
    s_settings_save_bottom = nullptr;
    s_settings_dirty       = false;

    lv_obj_t *c = content_ptr();

    lv_obj_set_layout(c, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_column(c, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(c, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(c, 0, LV_PART_MAIN);

    lv_obj_t *hdr = lv_obj_create(c);
    lv_obj_set_width(hdr, lv_pct(100));
    lv_obj_set_height(hdr, 52);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(hdr, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(hdr, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(hdr, 0, LV_PART_MAIN);
    lv_obj_set_layout(hdr, LV_LAYOUT_NONE);

    lv_obj_t *ttl = lv_label_create(hdr);
    lv_label_set_text(ttl, "Settings");
    lv_obj_set_style_text_font(ttl, APP_FONT_TITLE, LV_PART_MAIN);
    lv_obj_set_style_text_color(ttl, lv_color_hex(APP_C_SHEET_TEXT), LV_PART_MAIN);
    lv_obj_align(ttl, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *save_top = lv_button_create(hdr);
    settings_style_save_btn(save_top);
    lv_obj_t *stl = lv_label_create(save_top);
    lv_label_set_text(stl, "Save changes");
    lv_obj_set_style_text_font(stl, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(stl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(stl);
    lv_obj_add_event_cb(save_top, settings_save_click_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(save_top, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(save_top, LV_ALIGN_RIGHT_MID, -2, 0);
    s_settings_save_top = save_top;

    lv_obj_t *body = lv_obj_create(c);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(body, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(body, 0, LV_PART_MAIN);
    lv_obj_set_layout(body, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(body, 14, LV_PART_MAIN);

    lv_obj_t *sidebar = lv_obj_create(body);
    lv_obj_set_size(sidebar, 212, lv_pct(100));
    lv_obj_set_style_bg_opa(sidebar, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(sidebar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(sidebar, 0, LV_PART_MAIN);
    lv_obj_set_layout(sidebar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(sidebar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sidebar, 10, LV_PART_MAIN);
    lv_obj_remove_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(body);
    lv_obj_set_flex_grow(panel, 1);
    lv_obj_set_height(panel, lv_pct(100));
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);

    auto mk_side = [&](const char *sym, const char *caption, SettingsCat cat) {
        lv_obj_t *b = lv_button_create(sidebar);
        lv_obj_set_width(b, lv_pct(100));
        lv_obj_set_height(b, LV_SIZE_CONTENT);
        settings_sidebar_btn_style(b, s_settings_cat == cat);
        lv_obj_set_layout(b, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_min_height(b, 108, LV_PART_MAIN);
        lv_obj_set_style_pad_ver(b, 18, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(b, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_row(b, 6, LV_PART_MAIN);

        lv_obj_t *ic = lv_label_create(b);
        lv_label_set_text(ic, sym);
        lv_obj_set_style_text_font(ic, APP_FONT_HERO, LV_PART_MAIN);
        lv_obj_set_style_text_color(ic, lv_color_hex(APP_C_ICON_ACCENT_BLUE), LV_PART_MAIN);
        lv_obj_set_style_text_align(ic, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

        lv_obj_t *cap = lv_label_create(b);
        lv_label_set_text(cap, caption);
        lv_obj_set_style_text_font(cap, APP_FONT_BODY, LV_PART_MAIN);
        lv_obj_set_style_text_color(cap, lv_color_hex(APP_C_SHEET_TEXT_MUTE), LV_PART_MAIN);
        lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_width(cap, lv_pct(100));
        lv_label_set_long_mode(cap, LV_LABEL_LONG_MODE_DOTS);

        lv_obj_add_event_cb(
            b,
            [](lv_event_t *e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                s_settings_cat = (SettingsCat)(uintptr_t)lv_event_get_user_data(e);
                /* Refresh sidebar active state + right panel */
                lv_obj_t *sb = lv_obj_get_parent((lv_obj_t *)lv_event_get_target(e));
                const uint32_t n = lv_obj_get_child_count(sb);
                for (uint32_t i = 0; i < n; ++i) {
                    lv_obj_t *ch = lv_obj_get_child(sb, i);
                    if (!ch) continue;
                    SettingsCat ccat = (SettingsCat)(uintptr_t)lv_obj_get_user_data(ch);
                    settings_sidebar_btn_style(ch, ccat == s_settings_cat);
                }
                lv_obj_t *root = lv_obj_get_parent(sb);
                lv_obj_t *pn   = lv_obj_get_child(root, 1);
                settings_build_content(pn);
            },
            LV_EVENT_CLICKED, (void *)(uintptr_t)cat);
        lv_obj_set_user_data(b, (void *)(uintptr_t)cat);
        return b;
    };

    mk_side(LV_SYMBOL_CHARGE, "Power", SettingsCat::Power);
    mk_side(LV_SYMBOL_WIFI, "Comms", SettingsCat::Comms);
    mk_side(LV_SYMBOL_OK, "Security", SettingsCat::Security);
    mk_side(LV_SYMBOL_EYE_OPEN, "Sensors", SettingsCat::Sensors);

    /* Add existing sliders to Power category content */
    if (s_cpu_mhz != 80 && s_cpu_mhz != 160 && s_cpu_mhz != 240) s_cpu_mhz = 240;
    M5.update();
    if (s_backlight_restore < 1)
        s_backlight_restore = 60;

    settings_build_content(panel);
}
