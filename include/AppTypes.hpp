#pragma once
#include <cstdint>

/* Logical screens for app_show(); Calibrate + Splash have no global shell. */
enum class AppScreen : uint8_t {
    Calibrate,
    Splash,
    Home,
    Files,
    WiFi,
    Camera,
    Imu,
    Power,
    Sd,
    TouchTest,
    I2C,
    Rtc,
    Settings,
    Status, /* system status: CPU, storage, devices, clock sync */
    TouchCalibrate, /* 4-point cal; same NVS save as first-boot, returns Home when done */
    PowerMenu,      /* system power sheet from top bar (separate from Power app tile) */
    COUNT
};

void app_show(AppScreen s, void *user_ctx = nullptr);
void app_init();
