/**
 * Boot: Serial + short CDC delay → M5 (full brightness) → LVGL HAL → app_init() → splash.
 * Loop: keep M5 inputs fresh, run LVGL under a single lock.
 */
#include <Arduino.h>
#include <M5Unified.h>
#include "AppTypes.hpp"
#include "LvglHal.hpp"
#include "app_config.h"
#include "arcane_lvgl.h"

void setup() {
    Serial.begin(115200);
    delay(150); /* Step 6: USB CDC attach before M5 init (Tab5 / P4) */
    /*
     * Tab5 (P4 + C6 over SDIO): avoid bringing up STA in setup() so Wi-Fi init does not run before
     * splash `Speaker.begin()`/audio. User connects from the Wi-Fi screen; credentials stay in NVS.
     */
    m5::M5Unified::config_t m5c = m5::M5Unified::config();
#if APP_BOOT_NO_INTERNAL_MIC
    m5c.internal_mic = false;
#endif
    M5.begin(m5c);
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
