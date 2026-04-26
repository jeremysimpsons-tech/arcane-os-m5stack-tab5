/**
 * Boot: Serial + short CDC delay → M5 (full brightness) → LVGL HAL → app_init() → splash.
 * Loop: keep M5 inputs fresh, run LVGL under a single lock.
 */
#include <Arduino.h>
#include <M5Unified.h>
#include "AppTypes.hpp"
#include "LvglHal.hpp"
#include "arcane_lvgl.h"
#include "arc_wifi.hpp"

void setup() {
    Serial.begin(115200);
    delay(150); /* Step 6: USB CDC attach before M5 init (Tab5 / P4) */
    M5.begin();
    arc_wifi_begin_autoconnect();
    M5.Display.setBrightness((uint8_t)((255 * 60) / 100)); /* ~60% */
    LvglHal::init();
    LvglHal::lock();
    app_init();
    LvglHal::unlock();
}

void loop() {
    M5.update();
    LvglHal::lock();
    lv_timer_handler();
    LvglHal::unlock();
    delay(5);
}
