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
#include <Preferences.h>
#include <stdio.h>
#include <string.h>
#include "esp_system.h"


static void settings_reset_cal(lv_event_t *e) {
    (void)e;
    LvglHal::click_feedback();
    LvglHal::clear_touch_cal_from_nvs();
    esp_restart();
}

/* Power: allowed timeouts (seconds). Index matches dropdown order. */
static const uint32_t kPowerTimeoutS[] = {0u, 5u, 10u, 30u, 60u, 120u, 300u, 600u};
#define K_POWER_TIMEOUT_COUNT ((int)(sizeof kPowerTimeoutS / sizeof kPowerTimeoutS[0]))

static bool power_timeout_valid_s(uint32_t s) {
    for (size_t i = 0; i < sizeof kPowerTimeoutS / sizeof kPowerTimeoutS[0]; i++) {
        if (kPowerTimeoutS[i] == s) {
            return true;
        }
    }
    return false;
}

static int power_timeout_s_to_idx(int s) {
    for (int i = 0; i < K_POWER_TIMEOUT_COUNT; i++) {
        if ((int)kPowerTimeoutS[i] == s) {
            return i;
        }
    }
    return 0;
}

void arc_settings_load_from_nvs() {
    Preferences p;
    if (!p.begin(ARC_SETTINGS_NVS, true))
        return;
    if (p.getUInt("magic", 0) != ARC_SETTINGS_MAGIC) {
        p.end();
        return;
    }
    const uint32_t bl = p.getUInt("bl_s", 0);
    if (power_timeout_valid_s(bl)) {
        s_bl_timeout_s         = (int)bl;
        s_backlight_timeout_ms = bl * 1000u;
    }
    {
        const uint32_t dim = p.getUInt("dim_s", 255u);
        if (power_timeout_valid_s(dim)) {
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
    {
        /* Startup sound volume is independent from system volume. */
        uint32_t svol = p.getUInt("svol", 101u);
        if (svol > 100u) {
            svol = (uint32_t)((100u * (uint32_t)APP_SPLASH_VOLUME) / 255u);
            if (svol > 100u) svol = 100u;
        }
        s_startup_volume_pct = (uint8_t)svol;
    }
    s_ui_dark_mode = p.getBool("dark", false);
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
    p.putUInt("svol", (uint32_t)s_startup_volume_pct);
    p.putBool("dark", s_ui_dark_mode);
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

void settings_mark_dirty() {
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
        lv_obj_set_style_bg_color(b, lv_color_hex(on ? APP_C_ACCENT : ui_cpu_chip_idle()), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_t *lb = lv_obj_get_child(b, 0);
        if (lb)
            lv_obj_set_style_text_color(lb, lv_color_hex(on ? 0xFFFFFF : ui_text_primary()), LV_PART_MAIN);
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
    lv_obj_set_style_text_color(hdr, lv_color_hex(ui_text_primary()), LV_PART_MAIN);

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

void settings_sidebar_btn_style(lv_obj_t *b, bool active) {
    lv_obj_set_style_radius(b, 18, LV_PART_MAIN);
    lv_obj_set_style_border_width(b, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
    if (s_ui_dark_mode)
        lv_obj_set_style_bg_color(b, lv_color_hex(active ? 0x2C3F5A : 0x1C1C1E), LV_PART_MAIN);
    else
        lv_obj_set_style_bg_color(b, lv_color_hex(active ? 0xE8F1FB : 0xF2F2F7), LV_PART_MAIN);
}

void settings_content_title(lv_obj_t *parent, const char *t) {
    lv_obj_t *lab = lv_label_create(parent);
    lv_label_set_text(lab, t);
    lv_obj_set_width(lab, lv_pct(100));
    lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(lab, APP_FONT_HEADING, LV_PART_MAIN);
    lv_obj_set_style_text_color(lab, lv_color_hex(ui_text_primary()), LV_PART_MAIN);
}

static void settings_style_dropdown(lv_obj_t *dd) {
    if (!s_ui_dark_mode)
        return;
    lv_obj_set_style_bg_opa(dd, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dd, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_color(dd, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_t *lst = lv_dropdown_get_list(dd);
    if (lst) {
        lv_obj_set_style_bg_opa(lst, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(lst, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_color(lst, lv_color_hex(0x000000), LV_PART_MAIN);
    }
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
    lv_obj_set_style_text_color(l, lv_color_hex(ui_text_primary()), LV_PART_MAIN);
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
    settings_style_dropdown(dd);
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
    lv_obj_set_style_text_color(l, lv_color_hex(ui_text_primary()), LV_PART_MAIN);

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
                                   void (*on_set)(int), const char *val_fmt, const char *hint1 = nullptr,
                                   const char *hint2 = nullptr) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_width(box, lv_pct(100));
    app_style_muted_card(box);
    lv_obj_set_style_pad_all(box, 10, LV_PART_MAIN);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(box, LV_SCROLLBAR_MODE_OFF);
    /* Recreate slider rows without flex; match the working debug slider behavior. */
    lv_obj_set_layout(box, LV_LAYOUT_NONE);

    lv_obj_t *l = lv_label_create(box);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_font(l, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(ui_text_primary()), LV_PART_MAIN);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *val = lv_label_create(box);
    lv_obj_set_style_text_font(val, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(val, lv_color_hex(ui_text_mute()), LV_PART_MAIN);
    lv_obj_align(val, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_obj_t *sl = lv_slider_create(box);
    lv_obj_set_width(sl, lv_pct(100));
    /* Slim iOS-like slider: thin track + small knob. */
    constexpr int kSliderH   = 28;
    constexpr int kKnob      = 18;
    constexpr int kRowTopPad = 30;
    lv_obj_set_height(sl, (lv_coord_t)kSliderH);
    lv_obj_align(sl, LV_ALIGN_TOP_LEFT, 0, (lv_coord_t)kRowTopPad);
    lv_slider_set_range(sl, minv, maxv);
    lv_slider_set_value(sl, v, LV_ANIM_OFF);
    /* Style like debug slider (explicit selectors). */
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(sl, lv_color_hex(s_ui_dark_mode ? 0x2C2C2E : 0xC7C7CC), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(sl, lv_color_hex(APP_C_ICON_ACCENT_BLUE), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(sl, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_DEFAULT);
    /* Thin track + compact knob. */
    lv_obj_set_style_pad_all(sl, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(sl, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(sl, LV_RADIUS_CIRCLE, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(sl, LV_RADIUS_CIRCLE, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(sl, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(sl, lv_color_hex(s_ui_dark_mode ? 0x636366 : 0x8E8E93), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_width(sl, kKnob, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_height(sl, kKnob, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(sl, 1, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(sl, lv_color_hex(s_ui_dark_mode ? 0x8E8E93 : 0x3A3A3C), LV_PART_KNOB | LV_STATE_DEFAULT);
    /* Keep default interaction flags; removing SCROLLABLE can break drag on some LVGL builds. */
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

    int y = kRowTopPad + kSliderH + 10;
    if (hint1 && hint1[0]) {
        lv_obj_t *h1 = lv_label_create(box);
        lv_label_set_text(h1, hint1);
        lv_obj_set_width(h1, lv_pct(100));
        lv_label_set_long_mode(h1, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_style_text_font(h1, APP_FONT_SUB, LV_PART_MAIN);
        lv_obj_set_style_text_color(h1, lv_color_hex(ui_text_mute()), LV_PART_MAIN);
        lv_obj_align(h1, LV_ALIGN_TOP_LEFT, 0, y);
        y += 28;
    }
    if (hint2 && hint2[0]) {
        lv_obj_t *h2 = lv_label_create(box);
        lv_label_set_text(h2, hint2);
        lv_obj_set_width(h2, lv_pct(100));
        lv_label_set_long_mode(h2, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_style_text_font(h2, APP_FONT_SUB, LV_PART_MAIN);
        lv_obj_set_style_text_color(h2, lv_color_hex(ui_text_mute()), LV_PART_MAIN);
        lv_obj_align(h2, LV_ALIGN_TOP_LEFT, 0, y);
        y += 28;
    }
    lv_obj_set_height(box, (lv_coord_t)(y + 6));
}

static void rtc_mbox_ok_cb(lv_event_t *e) {
    lv_obj_t *mb = (lv_obj_t *)lv_event_get_user_data(e);
    if (mb && lv_obj_is_valid(mb)) {
        lv_msgbox_close(mb);
    }
}

static void rtc_sync_msg(const char *method) {
    lv_obj_t *mb = lv_msgbox_create(nullptr);
    lv_msgbox_add_title(mb, "RTC time sync");
    lv_msgbox_add_text(mb, method);
    lv_obj_t *bok = lv_msgbox_add_footer_button(mb, "OK");
    ui_mbox_bind_children_clicked(bok, rtc_mbox_ok_cb, mb);
    ui_msgbox_add_close_and_backdrop(mb);
    lv_obj_center(mb);
}

/** When true, next `view_settings()` keeps dirty state (e.g. after dark mode UI refresh). */
static bool s_settings_skip_dirty_clear = false;

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

        settings_add_slider_row(panel, "Volume", 0, 100, (int)s_volume_pct,
                                [](int pct) {
                                    s_volume_pct = (uint8_t)pct;
                                    M5.Speaker.setVolume((uint8_t)((255u * (uint32_t)pct) / 100u));
                                    settings_mark_dirty();
                                },
                                "%d%%");

        settings_add_slider_row(panel, "Startup sound volume", 0, 100, (int)s_startup_volume_pct,
                                [](int pct) {
                                    s_startup_volume_pct = (uint8_t)pct;
                                    settings_mark_dirty();
                                },
                                "%d%%");

        settings_add_dropdown_row(panel, "Backlight timeout",
                                  "Off\n5 seconds\n10 seconds\n30 seconds\n"
                                  "1 minute\n2 minutes\n5 minutes\n10 minutes",
                                  power_timeout_s_to_idx(s_bl_timeout_s),
                                  [](int sel) {
                                      if (sel < 0 || sel >= K_POWER_TIMEOUT_COUNT) {
                                          sel = 0;
                                      }
                                      s_bl_timeout_s         = (int)kPowerTimeoutS[sel];
                                      s_backlight_timeout_ms = (uint32_t)s_bl_timeout_s * 1000u;
                                      user_activity_poke();
                                      settings_mark_dirty();
                                  });

        settings_add_dropdown_row(panel, "Screen dim timeout",
                                  "Off\n5 seconds\n10 seconds\n30 seconds\n"
                                  "1 minute\n2 minutes\n5 minutes\n10 minutes",
                                  power_timeout_s_to_idx(s_dim_timeout_s),
                                  [](int sel) {
                                      if (sel < 0 || sel >= K_POWER_TIMEOUT_COUNT) {
                                          sel = 0;
                                      }
                                      s_dim_timeout_s  = (int)kPowerTimeoutS[sel];
                                      s_dim_timeout_ms  = (uint32_t)s_dim_timeout_s * 1000u;
                                      user_activity_poke();
                                      settings_mark_dirty();
                                  });

        settings_add_cpu_mhz_row(panel);
    } else if (s_settings_cat == SettingsCat::Display) {
        settings_content_title(panel, "Display");

        settings_add_slider_row(panel, "Display brightness", 5, 100, (int)s_backlight_restore,
                                [](int pct) {
                                    s_backlight_restore = (uint8_t)pct;
                                    M5.Display.setBrightness((uint8_t)((255u * (uint32_t)pct) / 100u));
                                    user_activity_poke();
                                    settings_mark_dirty();
                                },
                                "%d%%");

        settings_add_switch_row(panel, "Dark mode", s_ui_dark_mode, [](bool on) {
            s_ui_dark_mode = on;
            settings_mark_dirty();
            s_settings_skip_dirty_clear = true;
            lv_timer_t *tm = lv_timer_create(
                [](lv_timer_t *t) {
                    app_show(AppScreen::Settings);
                    lv_timer_delete(t);
                },
                30, nullptr);
            lv_timer_set_repeat_count(tm, 1);
        });
    } else if (s_settings_cat == SettingsCat::Comms) {
        settings_content_title(panel, "Comms");

        settings_add_slider_row(panel, "LoRa spreading factor", 0, 100, s_lora_mix,
                                [](int v) {
                                    s_lora_mix = v;
                                    settings_mark_dirty();
                                },
                                "%d", "Left: High range / low speed", "Right: Low range / high speed");

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
        lv_obj_set_style_text_color(t, lv_color_hex(ui_text_primary()), LV_PART_MAIN);
    } else { /* Sensors */
        settings_content_title(panel, "Sensors");

        lv_obj_t *box = lv_obj_create(panel);
        lv_obj_set_width(box, lv_pct(100));
        app_style_muted_card(box);

        lv_obj_t *t = lv_label_create(box);
        lv_label_set_text(t, "RTC time sync");
        lv_obj_set_style_text_font(t, APP_FONT_BODY, LV_PART_MAIN);
        lv_obj_set_style_text_color(t, lv_color_hex(ui_text_primary()), LV_PART_MAIN);

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
        mk("Internet", "Internet: sync RTC via NTP when Wi-Fi is connected (TODO).");
    }

    settings_install_save_footer(panel);
}

void view_settings() {
    shell_mount("Settings", nullptr, nullptr, false);
    s_settings_save_top    = nullptr;
    s_settings_save_bottom = nullptr;
    if (!s_settings_skip_dirty_clear)
        s_settings_dirty = false;
    s_settings_skip_dirty_clear = false;

    lv_obj_t *c = content_ptr();

    lv_obj_set_layout(c, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_column(c, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(c, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(c, 0, LV_PART_MAIN);

    /* Title in shell top bar; keep only Save on the first row. */
    lv_obj_t *hdr = lv_obj_create(c);
    lv_obj_set_width(hdr, lv_pct(100));
    lv_obj_set_height(hdr, 52);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(hdr, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(hdr, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(hdr, 0, LV_PART_MAIN);
    lv_obj_set_layout(hdr, LV_LAYOUT_NONE);

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
        lv_obj_set_style_text_color(cap, lv_color_hex(ui_text_mute()), LV_PART_MAIN);
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
    mk_side(LV_SYMBOL_IMAGE, "Display", SettingsCat::Display);
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
