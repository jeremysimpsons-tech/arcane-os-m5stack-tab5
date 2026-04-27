#pragma once
#include <cstdint>

/* Logical screens for app_show(); Splash has no global shell. */
enum class AppScreen : uint8_t {
    Splash,
    Home,
    Files,
    Camera,
    Imu,
    Power,
    Sd,
    TouchTest,
    I2C,
    Rtc,
    Settings,
    Status, /* system status: CPU, storage, devices, clock sync */
    WiFi,   /* C6: Tab5M5Comms (M5 style bridge), not WiFi.h in app sources */
    Ogsm,   /* Off Grid Secure Messaging */
    PowerMenu,      /* system power sheet from top bar (separate from Power app tile) */
    COUNT
};

void app_show(AppScreen s, void *user_ctx = nullptr);
void app_init();
