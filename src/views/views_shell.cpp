#include "Views.hpp"
#include "AppTypes.hpp"
#include "LvglHal.hpp"
#include "views_config.hpp"
#include "views_internal.hpp"
#include "app_config.h"
#include "app_theme.hpp"
#include "arcane_lvgl.h"
#include "Tab5M5Comms.hpp"
#include "arc_wifi.hpp"
#include <M5Unified.h>
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <WiFiType.h>

extern "C" const lv_img_dsc_t wolf;


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

void power_off_do_cb(lv_event_t *e) {
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
    if (s_shell_batt.theme && lv_obj_is_valid(s_shell_batt.theme)) {
        lv_label_set_text(s_shell_batt.theme, s_ui_dark_mode ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
        lv_obj_set_style_text_color(s_shell_batt.theme, lv_color_hex(APP_C_TEXT), LV_PART_MAIN);
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

void shell_wifi_refresh() {
    lv_obj_t *w = s_shell_wifi.wrap;
    if (!w || !lv_obj_is_valid(w)) {
        return;
    }
    if (!M5Comms.isReady()) {
        lv_obj_add_flag(w, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_remove_flag(w, LV_OBJ_FLAG_HIDDEN);
    const int st = M5Comms.WiFi.status();
    if (st != WL_CONNECTED) {
        lv_label_set_text(s_shell_wifi.ic, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(s_shell_wifi.ic, lv_color_hex(0x8E8E93), LV_PART_MAIN);
        lv_obj_set_style_text_decor(s_shell_wifi.ic, LV_TEXT_DECOR_NONE, LV_PART_MAIN);
        /* 256 = 1× in LVGL; avoid large scale — overlaps neighbours. */
        lv_obj_set_style_transform_scale(s_shell_wifi.ic, 256, LV_PART_MAIN);
        lv_obj_add_flag(s_shell_wifi.bars, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_label_set_text(s_shell_wifi.ic, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(s_shell_wifi.ic, lv_color_hex(APP_C_TEXT), LV_PART_MAIN);
    int r = M5Comms.WiFi.connectedRSSI();
    int sc = 256;
    if (r > -55) {
        sc = 280;
    } else if (r > -70) {
        sc = 268;
    }
    lv_obj_set_style_transform_scale(s_shell_wifi.ic, sc, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_x(s_shell_wifi.ic, 8, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(s_shell_wifi.ic, 8, LV_PART_MAIN);
    lv_obj_remove_flag(s_shell_wifi.bars, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_shell_wifi.bars, LV_SYMBOL_BARS);
    lv_obj_set_style_text_color(s_shell_wifi.bars, lv_color_hex(APP_C_TEXT), LV_PART_MAIN);
    int bsc = 256;
    if (r > -55) {
        bsc = 276;
    } else if (r > -70) {
        bsc = 266;
    }
    lv_obj_set_style_transform_scale(s_shell_wifi.bars, bsc, LV_PART_MAIN);
    arc_net_poll();
    const bool online = arc_net_is_online();
    if (online) {
        lv_obj_set_style_text_decor(s_shell_wifi.ic, LV_TEXT_DECOR_NONE, LV_PART_MAIN);
        lv_obj_set_style_text_decor(s_shell_wifi.bars, LV_TEXT_DECOR_NONE, LV_PART_MAIN);
    } else {
        lv_obj_set_style_text_decor(s_shell_wifi.ic, LV_TEXT_DECOR_STRIKETHROUGH, LV_PART_MAIN);
        lv_obj_set_style_text_decor(s_shell_wifi.bars, LV_TEXT_DECOR_STRIKETHROUGH, LV_PART_MAIN);
    }
}

static void shell_batt_timer_cb(lv_timer_t *t) {
    (void)t;
    shell_batt_refresh_all();
    shell_wifi_refresh();
}

void shell_style_topbar_btn(lv_obj_t *btn) {
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN);
}

/* ---------- Backlight timeout (Settings); state in views_state.cpp ---------- */
void user_activity_poke() {
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
void home_icon_center_pivot(lv_obj_t *lbl) {
    const lv_coord_t w = lv_obj_get_width(lbl);
    const lv_coord_t h = lv_obj_get_height(lbl);
    if (w <= 0 || h <= 0)
        return;
    lv_obj_set_style_transform_pivot_x(lbl, w / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(lbl, h / 2, LV_PART_MAIN);
}

void home_icon_size_cb(lv_event_t *e) {
    home_icon_center_pivot((lv_obj_t *)lv_event_get_target(e));
}

void shell_mount(const char *title, void (*on_right)(lv_event_t *), const char *right_caption, bool home_shell) {
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
        lv_obj_set_style_bg_opa(br, LV_OPA_TRANSP, LV_STATE_PRESSED);
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
    lv_obj_set_style_pad_column(brow, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(brow, 0, LV_PART_MAIN);
    lv_obj_remove_flag(brow, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *wifiw = lv_obj_create(brow);
    lv_obj_set_size(wifiw, LV_SIZE_CONTENT, side_btn);
    lv_obj_set_style_bg_opa(wifiw, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(wifiw, 0, LV_PART_MAIN);
    lv_obj_set_layout(wifiw, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(wifiw, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wifiw, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(wifiw, 2, LV_PART_MAIN);
    /* Gap before dark-mode / battery cluster (Wi-Fi glyphs can scale and need room). */
    lv_obj_set_style_pad_right(wifiw, 10, LV_PART_MAIN);
    lv_obj_remove_flag(wifiw, LV_OBJ_FLAG_SCROLLABLE);
    s_shell_wifi.wrap = wifiw;
    s_shell_wifi.ic   = lv_label_create(wifiw);
    lv_label_set_text(s_shell_wifi.ic, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(s_shell_wifi.ic, APP_FONT_SUB, LV_PART_MAIN);
    s_shell_wifi.bars = lv_label_create(wifiw);
    lv_label_set_text(s_shell_wifi.bars, LV_SYMBOL_BARS);
    lv_obj_set_style_text_font(s_shell_wifi.bars, APP_FONT_CAP, LV_PART_MAIN);
    shell_wifi_refresh();

    lv_obj_t *theme_ic = lv_label_create(brow);
    lv_label_set_text(theme_ic, s_ui_dark_mode ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_font(theme_ic, APP_FONT_HEADING, LV_PART_MAIN);
    lv_obj_set_style_text_color(theme_ic, lv_color_hex(APP_C_TEXT), LV_PART_MAIN);
    lv_obj_set_style_pad_left(theme_ic, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_right(theme_ic, 4, LV_PART_MAIN);

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
    s_shell_batt.theme = theme_ic;
    s_shell_batt.chrg  = batt_chrg;
    s_shell_batt.sym   = batt_sym;
    s_shell_batt.num   = batt_num;
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

    /* Title: in-app stays centered in the safe band; home title is left-led from the logo so it
     * does not crowd the right-side status / battery cluster. */
    lv_obj_t *tlab = lv_label_create(s_bar);
    lv_label_set_text(tlab, (title && title[0]) ? title : APP_NAME);
    lv_obj_set_style_text_font(tlab, APP_FONT_HEADING, LV_PART_MAIN);
    lv_obj_set_style_text_color(tlab, lv_color_hex(APP_C_TEXT), LV_PART_MAIN);
    lv_label_set_long_mode(tlab, LV_LABEL_LONG_MODE_DOTS);
    if (home_shell) {
        lv_obj_set_style_text_align(tlab, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        /* Start just after the wolf tile; cap width so text does not run under the right cluster. */
        const lv_coord_t left_inset = side_btn + 14;
        lv_obj_set_width(tlab, (lv_coord_t)(APP_LCD_WIDTH - left_inset - 220));
        lv_obj_align(tlab, LV_ALIGN_LEFT_MID, left_inset, 0);
    } else {
        lv_obj_set_style_text_align(tlab, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_width(tlab, (lv_coord_t)(APP_LCD_WIDTH - (side_btn + 34) * 2));
        lv_obj_align(tlab, LV_ALIGN_CENTER, 0, 0);
    }
    s_shell_batt_timer = lv_timer_create(shell_batt_timer_cb, 1200, nullptr);
    lv_timer_set_repeat_count(s_shell_batt_timer, -1);

    s_cbody = lv_obj_create(scr);
    lv_obj_set_width(s_cbody, APP_LCD_WIDTH);
    lv_obj_set_flex_grow(s_cbody, 1);
    lv_obj_set_style_margin_top(s_cbody, 0, LV_PART_MAIN);
    /* Unified app background: match Home (white) */
    color_bg(s_cbody, ui_bg_content());
    lv_obj_set_style_pad_all(s_cbody, (lv_coord_t)APP_SAFE_INSET_V, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_cbody, 8, LV_PART_MAIN);
    lv_obj_set_style_radius(s_cbody, 0, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(s_cbody, false, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_cbody, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(s_cbody, 0, LV_PART_MAIN);
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

lv_obj_t *content_ptr() { return s_cbody; }

