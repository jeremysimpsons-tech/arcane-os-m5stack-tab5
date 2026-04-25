#include "AppTypes.hpp"
#include "LvglHal.hpp"
#include "Views.hpp"
#include <Arduino.h>

void app_show(AppScreen s, void *user_ctx) {
    /* Always call from LVGL context while the main loop holds LvglHal::lock(), or from setup with same lock. */
    switch (s) {
    case AppScreen::Calibrate:
        view_calibrate();
        break;
    case AppScreen::Splash:
        view_splash();
        break;
    case AppScreen::Home:
        view_home();
        break;
    case AppScreen::Files:
        view_files(user_ctx);
        break;
    case AppScreen::WiFi:
        view_wifi();
        break;
    case AppScreen::Camera:
        view_camera();
        break;
    case AppScreen::Imu:
        view_imu();
        break;
    case AppScreen::Power:
        view_power();
        break;
    case AppScreen::Sd:
        view_sd();
        break;
    case AppScreen::TouchTest:
        view_touchtest();
        break;
    case AppScreen::I2C:
        view_i2c();
        break;
    case AppScreen::Rtc:
        view_rtc();
        break;
    case AppScreen::Status:
        view_status();
        break;
    case AppScreen::Settings:
        view_settings();
        break;
    case AppScreen::TouchCalibrate:
        view_touch_calibrate();
        break;
    case AppScreen::PowerMenu:
        view_power_menu();
        break;
    default:
        view_home();
        break;
    }
}

void app_init() {
    arc_settings_load_from_nvs();
    /* Boot flow:
       1) device boots
       2) splashscreen (animated)
       3) tap anywhere → Home */
    view_splash();
}
