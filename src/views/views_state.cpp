#include "views_internal.hpp"
#include "views_config.hpp"

bool     s_ui_dark_mode = false;

bool   s_splash_dismissed = false;

lv_obj_t *s_cbody                = nullptr;
ShellBatt  s_shell_batt  = {};
ShellTopWifi s_shell_wifi = {};
lv_timer_t *s_shell_batt_timer   = nullptr;

uint32_t s_backlight_timeout_ms = 0;
uint32_t s_dim_timeout_ms      = 0;
uint32_t s_last_activity_ms     = 0;
bool     s_backlight_dimmed     = false;
bool     s_soft_dimmed         = false;
uint8_t  s_backlight_restore    = 60;
lv_timer_t *s_idle_timer        = nullptr;

bool s_settings_skip_dirty  = false;
int  s_cpu_mhz              = 240;
int  s_lora_mix              = 50;
bool s_mesh_gateway          = false;
int  s_bl_timeout_s         = 0;
int  s_dim_timeout_s        = 0;
uint8_t s_volume_pct         = 70;
bool  s_settings_dirty      = false;
lv_obj_t *s_settings_save_top    = nullptr;
lv_obj_t *s_settings_save_bottom = nullptr;

SettingsCat s_settings_cat = SettingsCat::Power;
StatusCat   s_status_cat   = StatusCat::System;
