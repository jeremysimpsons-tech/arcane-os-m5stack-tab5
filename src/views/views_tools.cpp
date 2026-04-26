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



void view_camera() {
    shell_mount("Camera", nullptr, nullptr, false);
    lv_obj_t *c = content_ptr();
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
    shell_mount("Power", views_noop_right, "Info", false);
    lv_obj_t *c = content_ptr();
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
    color_bg(wrap, ui_bg_content());
    lv_obj_set_style_border_width(wrap, 0, LV_PART_MAIN);
    lv_obj_set_layout(wrap, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(wrap, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(wrap, 24, LV_PART_MAIN);
    lv_obj_remove_flag(wrap, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn = lv_button_create(wrap);
    lv_obj_set_size(btn, 220, 200);
    color_bg(btn, ui_card_bg());
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
    shell_mount("I2C scan", views_noop_right, "Scan", false);
    lv_obj_t *c = content_ptr();
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
