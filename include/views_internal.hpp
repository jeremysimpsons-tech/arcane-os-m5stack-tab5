#pragma once

#include "app_config.h"
#include "app_theme.hpp"
#include "arcane_lvgl.h"
#include "AppTypes.hpp"
#include <lvgl.h>
#include <M5Unified.h>
#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <vector>
#include <WString.h>

/* ---------- Shared state (defined in src/views/views_state.cpp) ---------- */
struct ShellBatt {
    lv_obj_t *theme;
    lv_obj_t *chrg;
    lv_obj_t *sym;
    lv_obj_t *num;
};

/** Top bar: single `LV_SYMBOL_WIFI` — RSSI shown via scale + tint (low/mid/high); strikethrough when on AP but no internet. */
struct ShellTopWifi {
    lv_obj_t *wrap;
    lv_obj_t *ic;
};

/** OGSM: envelope + unread badge (hidden when zero). */
struct ShellTopOgsm {
    lv_obj_t *wrap;
    lv_obj_t *ic;
    lv_obj_t *badge;
};

extern bool     s_ui_dark_mode;

extern bool   s_splash_dismissed;

extern lv_obj_t *s_cbody;
extern ShellBatt    s_shell_batt;
extern ShellTopWifi s_shell_wifi;
extern ShellTopOgsm s_shell_ogsm;
extern lv_timer_t  *s_shell_batt_timer;

extern uint32_t s_backlight_timeout_ms;
extern uint32_t s_dim_timeout_ms;
extern uint32_t s_last_activity_ms;
extern bool     s_backlight_dimmed;
extern bool     s_soft_dimmed;
extern uint8_t  s_backlight_restore;
extern lv_timer_t *s_idle_timer;

extern bool s_settings_skip_dirty;
extern int  s_cpu_mhz;
extern int  s_lora_mix;
extern bool s_mesh_gateway;
extern int  s_bl_timeout_s;
extern int  s_dim_timeout_s;
extern uint8_t s_volume_pct;
extern uint8_t s_startup_volume_pct;
extern bool  s_settings_dirty;
extern lv_obj_t *s_settings_save_top;
extern lv_obj_t *s_settings_save_bottom;

enum class SettingsCat : uint8_t { Power = 0, Display = 1, Comms = 2, Security = 3, Sensors = 4 };
enum class StatusCat : uint8_t { System = 0, StorageRam = 1, Power = 2, Other = 3 };

extern SettingsCat s_settings_cat;
extern StatusCat   s_status_cat;

/* ---------- UI / shell API ---------- */
void          apply_scroll_tabled(lv_obj_t *o);
void          color_bg(lv_obj_t *o, uint32_t hex);
void          app_style_ios_slider(lv_obj_t *sl);
void          app_style_muted_card(lv_obj_t *o);
lv_obj_t *    app_screen_title(lv_obj_t *c, const char *title);
void          user_activity_poke();
uint32_t      ui_bg_content(void);
uint32_t      ui_text_primary(void);
uint32_t      ui_text_mute(void);
uint32_t      ui_card_bg(void);
uint32_t      ui_card_border(void);
uint32_t      ui_cpu_chip_idle(void);

void shell_mount(const char *title, void (*on_right)(lv_event_t *), const char *right_caption, bool home_shell);
void shell_style_topbar_btn(lv_obj_t *btn);
void power_off_do_cb(lv_event_t *e);
lv_obj_t *content_ptr();

/** Footer / header buttons with a label child only receive clicks on the label; bind both. */
void ui_mbox_bind_children_clicked(lv_obj_t *btn, lv_event_cb_t cb, void *ud);
/** `lv_msgbox_create(nullptr)` places `mb` on a full-screen backdrop; tap dim area to close. */
void ui_msgbox_install_backdrop_dismiss(lv_obj_t *mb);
void ui_msgbox_add_close_and_backdrop(lv_obj_t *mb);

void settings_sidebar_btn_style(lv_obj_t *b, bool active);
void settings_content_title(lv_obj_t *parent, const char *t);
void home_icon_center_pivot(lv_obj_t *lbl);
void home_icon_size_cb(lv_event_t *e);
void shell_wifi_refresh();
void shell_ogsm_refresh();

/* Tools / Files */
void views_noop_right(lv_event_t *e);
void files_action_sheet_cb(lv_event_t *e);

void settings_mark_dirty();

/* Status uses settings title helpers — already above */

/* External asset */
extern "C" const lv_img_dsc_t wolf;
