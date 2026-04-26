#include "Views.hpp"
#include "AppTypes.hpp"
#include "views_config.hpp"
#include "views_internal.hpp"
#include "app_config.h"
#include "app_theme.hpp"
#include "arcane_lvgl.h"
#include <M5Unified.h>
#include <Arduino.h>
#include <stdio.h>
#include <string.h>



void apply_scroll_tabled(lv_obj_t *o) {
    lv_obj_add_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_flag(o, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_AUTO);
}

void color_bg(lv_obj_t *o, uint32_t hex) {
    lv_obj_set_style_bg_color(o, lv_color_hex(hex), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
}

uint32_t ui_bg_content(void) {
    return s_ui_dark_mode ? 0x000000u : (uint32_t)APP_C_HOME_SCREEN;
}
uint32_t ui_text_primary(void) {
    return s_ui_dark_mode ? 0xF2F2F4u : (uint32_t)APP_C_SHEET_TEXT;
}
uint32_t ui_text_mute(void) {
    return s_ui_dark_mode ? 0x9898A0u : (uint32_t)APP_C_SHEET_TEXT_MUTE;
}
uint32_t ui_card_bg(void) {
    return s_ui_dark_mode ? 0x1C1C1Eu : 0xF2F2F7u;
}
uint32_t ui_card_border(void) {
    return s_ui_dark_mode ? 0x3A3A3Cu : 0xE5E5EAu;
}
uint32_t ui_cpu_chip_idle(void) {
    return s_ui_dark_mode ? 0x3A3A3Cu : 0xE8E8EDu;
}

void app_style_ios_slider(lv_obj_t *sl) {
    if (s_ui_dark_mode) {
        lv_obj_set_style_bg_color(sl, lv_color_hex(0x2C2C2E), LV_PART_MAIN);
        lv_obj_set_style_bg_color(sl, lv_color_hex(APP_C_ICON_ACCENT_BLUE), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(sl, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
        lv_obj_set_style_border_color(sl, lv_color_hex(0x636366), LV_PART_KNOB);
    } else {
        lv_obj_set_style_bg_color(sl, lv_color_hex(0xE5E5EA), LV_PART_MAIN);
        lv_obj_set_style_bg_color(sl, lv_color_hex(APP_C_ICON_ACCENT_BLUE), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(sl, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
        lv_obj_set_style_border_color(sl, lv_color_hex(0xD1D1D6), LV_PART_KNOB);
    }
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(sl, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(sl, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_border_width(sl, 1, LV_PART_KNOB);
}

lv_obj_t *app_screen_title(lv_obj_t *c, const char *title) {
    lv_obj_t *t = lv_label_create(c);
    lv_label_set_text(t, title);
    lv_obj_set_width(t, lv_pct(100));
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(t, APP_FONT_HERO, LV_PART_MAIN);
    lv_obj_set_style_text_color(t, lv_color_hex(ui_text_primary()), LV_PART_MAIN);
    return t;
}

void app_style_muted_card(lv_obj_t *o) {
    color_bg(o, ui_card_bg());
    lv_obj_set_style_radius(o, 16, LV_PART_MAIN);
    lv_obj_set_style_border_color(o, lv_color_hex(ui_card_border()), LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(o, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(o, 0, LV_PART_MAIN);
}
