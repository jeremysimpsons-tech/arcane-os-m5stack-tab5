#pragma once
#include "AppTypes.hpp"

/** Load persisted settings (CPU MHz, backlight timeout, etc.) from NVS; call once after M5.begin. */
void arc_settings_load_from_nvs();

/** Call from pointer input when user touches the panel (wakes backlight / clears dim). */
void arc_notify_pointer_activity(void);

void view_calibrate();
void view_splash();
void view_home();
void view_files(void *user_ctx = nullptr);
void view_wifi();
void view_camera();
void view_imu();
void view_power();
void view_sd();
void view_touchtest();
void view_i2c();
void view_rtc();
void view_status();
void view_settings();
void view_touch_calibrate();
void view_power_menu();
