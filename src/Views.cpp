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
#include <stdio.h>
#include <esp_system.h>

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

void view_splash() {
    s_splash_dismissed = false;
    lv_obj_t *scr = lv_screen_active();
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clean(scr);
    color_bg(scr, APP_C_SPLASH_BG);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, APP_NAME);
    lv_obj_set_style_text_font(title, APP_FONT_HERO, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(title);
    lv_obj_set_y(title, lv_obj_get_y(title) - 40);

    lv_obj_t *ver = lv_label_create(scr);
    {
        char b[64];
        snprintf(b, sizeof(b), "v%s  ·  " __DATE__ " " __TIME__, APP_VERSION);
        lv_label_set_text(ver, b);
    }
    lv_obj_set_style_text_font(ver, APP_FONT_SUB, LV_PART_MAIN);
    lv_obj_set_style_text_color(ver, lv_color_hex(0xA8A8AE), LV_PART_MAIN);
    lv_obj_align(ver, LV_ALIGN_CENTER, 0, 20);

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "Touch screen to continue");
    lv_obj_set_style_text_font(hint, APP_FONT_HEADING, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(APP_C_ACCENT), LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -48);

    splash_add_dismiss(scr);
    splash_add_dismiss(title);
    splash_add_dismiss(ver);
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
    lv_obj_t *sym;
    lv_obj_t *num;
};
static ShellBatt s_shell_batt = {};

static void shell_batt_refresh_pair(lv_obj_t *sym, lv_obj_t *num) {
    if (!sym || !num || !lv_obj_is_valid(sym) || !lv_obj_is_valid(num))
        return;
    M5.update();
    int           pct     = (int)M5.Power.getBatteryLevel();
    const bool    charging = (M5.Power.isCharging() == m5::Power_Class::is_charging);
    if (pct < 0 || pct > 100) {
        lv_label_set_text(sym, LV_SYMBOL_BATTERY_FULL);
        lv_label_set_text(num, " —");
        const lv_color_t c = lv_color_hex(0xD1D1D6);
        lv_obj_set_style_text_color(sym, c, LV_PART_MAIN);
        lv_obj_set_style_text_color(num, c, LV_PART_MAIN);
    } else {
        const char *bs = charging ? LV_SYMBOL_CHARGE : shell_battery_symbol(pct);
        lv_label_set_text(sym, bs);
        char nb[20];
        snprintf(nb, sizeof(nb), " %d%%", pct);
        lv_label_set_text(num, nb);
        shell_batt_apply_color2(sym, num, pct, charging);
    }
}

static void shell_batt_timer_cb(lv_timer_t *t) {
    (void)t;
    shell_batt_refresh_pair(s_shell_batt.sym, s_shell_batt.num);
}

static void shell_style_topbar_btn(lv_obj_t *btn) {
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN);
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

    const lv_coord_t side_btn = (lv_coord_t)(APP_TOPBAR_PX - 20);

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
        lv_obj_set_style_text_font(bl, APP_FONT_TITLE, LV_PART_MAIN);
        lv_obj_set_style_text_color(bl, lv_color_hex(APP_C_TEXT), LV_PART_MAIN);
        lv_obj_center(bl);
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

    lv_obj_t *batt_sym = lv_label_create(brow);
    lv_obj_set_style_text_font(batt_sym, APP_FONT_HEADING, LV_PART_MAIN);
    lv_obj_t *batt_num = lv_label_create(brow);
    lv_obj_set_style_text_font(batt_num, APP_FONT_HEADING, LV_PART_MAIN);
    s_shell_batt.sym = batt_sym;
    s_shell_batt.num = batt_num;
    shell_batt_refresh_pair(batt_sym, batt_num);

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
    s_shell_batt_timer = lv_timer_create(shell_batt_timer_cb, 2500, nullptr);
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
    lv_obj_set_style_text_font(t, APP_FONT_HEADING, LV_PART_MAIN);
    lv_obj_set_style_text_color(t, lv_color_hex(APP_C_ICON_GREY_DARK), LV_PART_MAIN);
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

/* ---------- Home: iPhone-like grid (4 cols, up to 6 rows) ---------- */
enum { HOME_ICON_COLS = 4, HOME_ICON_GAP = 22, HOME_ICON_PAD = 26 };

static const struct {
    const char *sym;
    const char *label;
    AppScreen   id;
    uint32_t    color;
} k_apps[] = {
    { LV_SYMBOL_SETTINGS, "Settings", AppScreen::Settings, APP_C_ICON_ACCENT_BLUE },
    { LV_SYMBOL_EDIT, "Touch cal", AppScreen::TouchCalibrate, APP_C_ICON_GREY_MID },
    { LV_SYMBOL_DIRECTORY, "Files", AppScreen::Files, APP_C_ICON_GREY_MID },
    { LV_SYMBOL_WIFI, "Wi‑Fi", AppScreen::WiFi, APP_C_ICON_ACCENT_BLUE },
    { LV_SYMBOL_VIDEO, "Camera", AppScreen::Camera, APP_C_ICON_GREY_DARK },
    { LV_SYMBOL_BARS, "IMU", AppScreen::Imu, APP_C_ICON_GREY_LIGHT },
    { LV_SYMBOL_CHARGE, "Power", AppScreen::Power, APP_C_ICON_GREY_MID },
    { LV_SYMBOL_DRIVE, "Storage", AppScreen::Sd, APP_C_ICON_GREY_DARK },
    { LV_SYMBOL_KEYBOARD, "Touch", AppScreen::TouchTest, APP_C_ICON_GREY_LIGHT },
    { LV_SYMBOL_SHUFFLE, "I2C", AppScreen::I2C, APP_C_ICON_GREY_MID },
    { LV_SYMBOL_LOOP, "RTC", AppScreen::Rtc, APP_C_ICON_GREY_LIGHT },
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
        lv_obj_set_style_bg_opa(icon_box, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(icon_box, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(icon_box, 0, LV_PART_MAIN);
        /* Bubble touch events to the parent hit target */
        lv_obj_add_flag(icon_box, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_remove_flag(icon_box, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *ic = lv_label_create(icon_box);
        lv_label_set_text(ic, k_apps[i].sym);
        lv_obj_set_style_text_font(ic, APP_FONT_HERO, LV_PART_MAIN);
        lv_obj_set_style_text_color(ic, lv_color_hex(k_apps[i].color), LV_PART_MAIN);
        lv_obj_set_style_text_align(ic, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_outline_stroke_width(ic, 2, LV_PART_MAIN);
        lv_obj_set_style_text_outline_stroke_opa(ic, LV_OPA_70, LV_PART_MAIN);
        lv_obj_set_style_text_outline_stroke_color(ic, lv_color_hex(0xE8ECF1), LV_PART_MAIN);
        lv_obj_add_flag(ic, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_center(ic);

        lv_obj_update_layout(ic);
        lv_obj_set_style_transform_pivot_x(ic, lv_obj_get_width(ic) / 2, LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(ic, lv_obj_get_height(ic) / 2, LV_PART_MAIN);
        lv_obj_set_style_transform_scale_x(ic, 256 * 3, LV_PART_MAIN);
        lv_obj_set_style_transform_scale_y(ic, 256 * 3, LV_PART_MAIN);

        lv_obj_t *cap = lv_label_create(hit);
        lv_label_set_text(cap, k_apps[i].label);
        lv_obj_set_style_text_font(cap, APP_FONT_HEADING, LV_PART_MAIN);
        lv_obj_set_style_text_color(cap, lv_color_hex(APP_C_ICON_GREY_DARK), LV_PART_MAIN);
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
    lv_obj_set_size(row, lv_pct(100), 48);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 8, LV_PART_MAIN);
    app_style_muted_card(row);
    for (const char *t : { "Internal", "SD card" }) {
        lv_obj_t *b = lv_button_create(row);
        lv_obj_set_flex_grow(b, 1);
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

void view_settings() {
    shell_mount("Settings", nullptr, nullptr, false);
    lv_obj_t *c = content_ptr();
    app_screen_title(c, "Settings");

    M5.update();
    int bright_pct = (int)((M5.Display.getBrightness() * 100) / 255);
    if (bright_pct < 1)
        bright_pct = 60;
    if (bright_pct > 100)
        bright_pct = 100;

    /* Brightness */
    lv_obj_t *l1 = lv_label_create(c);
    lv_label_set_text(l1, "Brightness");
    lv_obj_set_style_text_font(l1, APP_FONT_SUB, LV_PART_MAIN);
    lv_obj_set_style_text_color(l1, lv_color_hex(APP_C_ICON_GREY_DARK), LV_PART_MAIN);

    lv_obj_t *valb = lv_label_create(c);
    lv_obj_set_style_text_font(valb, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(valb, lv_color_hex(APP_C_ICON_GREY_MID), LV_PART_MAIN);

    lv_obj_t *slb = lv_slider_create(c);
    lv_obj_set_width(slb, lv_pct(100));
    lv_slider_set_range(slb, 1, 100);
    lv_slider_set_value(slb, bright_pct, LV_ANIM_OFF);
    app_style_ios_slider(slb);

    auto on_bright = [](lv_event_t *e) {
        lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
        lv_obj_t *label  = (lv_obj_t *)lv_event_get_user_data(e);
        int       pct    = (int)lv_slider_get_value(slider);
        char      b[24];
        snprintf(b, sizeof(b), "%d%%", pct);
        if (label) lv_label_set_text(label, b);
        M5.Display.setBrightness((uint8_t)((255 * pct) / 100));
    };
    lv_obj_add_event_cb(slb, on_bright, LV_EVENT_VALUE_CHANGED, valb);
    {
        char b[24];
        snprintf(b, sizeof(b), "%d%%", bright_pct);
        lv_label_set_text(valb, b);
    }

    /* Volume (speaker; Tab5 / M5Unified: master volume 0–255) */
    lv_obj_t *l2 = lv_label_create(c);
    lv_label_set_text(l2, "Volume");
    lv_obj_set_style_text_font(l2, APP_FONT_SUB, LV_PART_MAIN);
    lv_obj_set_style_text_color(l2, lv_color_hex(APP_C_ICON_GREY_DARK), LV_PART_MAIN);

    lv_obj_t *valv = lv_label_create(c);
    lv_obj_set_style_text_font(valv, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(valv, lv_color_hex(APP_C_ICON_GREY_MID), LV_PART_MAIN);

    int vol_pct = (int)((M5.Speaker.getVolume() * 100) / 255);
    if (vol_pct < 0)
        vol_pct = 0;
    if (vol_pct > 100)
        vol_pct = 100;

    lv_obj_t *slv = lv_slider_create(c);
    lv_obj_set_width(slv, lv_pct(100));
    lv_slider_set_range(slv, 0, 100);
    lv_slider_set_value(slv, vol_pct, LV_ANIM_OFF);
    app_style_ios_slider(slv);

    auto on_vol = [](lv_event_t *e) {
        lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
        lv_obj_t *label  = (lv_obj_t *)lv_event_get_user_data(e);
        int       pct    = (int)lv_slider_get_value(slider);
        char      b[24];
        snprintf(b, sizeof(b), "%d%%", pct);
        if (label) lv_label_set_text(label, b);
        M5.Speaker.setVolume((uint8_t)((255 * pct) / 100));
    };
    lv_obj_add_event_cb(slv, on_vol, LV_EVENT_VALUE_CHANGED, valv);
    {
        char b[24];
        snprintf(b, sizeof(b), "%d%%", vol_pct);
        lv_label_set_text(valv, b);
    }

    lv_obj_t *btn = lv_button_create(c);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xF2F2F7), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 16, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(0xE5E5EA), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_t *bl = lv_label_create(btn);
    lv_label_set_text(bl, "Reset touch calibration (reboots)");
    lv_obj_set_style_text_font(bl, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(bl, lv_color_hex(APP_C_ICON_GREY_DARK), LV_PART_MAIN);
    lv_obj_center(bl);
    lv_obj_add_event_cb(btn, settings_reset_cal, LV_EVENT_CLICKED, nullptr);
}
