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
#include "ogsm/ogsm_service.hpp"
#include "views_internal.hpp"
#include "views_config.hpp"
#include <Preferences.h>

static void audio_bootstrap() {
    /* After brownout/reboot during audio, the codec can be left in a bad state. Force re-init early. */
    uint32_t vol_pct = 70u;
    Preferences p;
    if (p.begin(ARC_SETTINGS_NVS, true) && p.getUInt("magic", 0) == ARC_SETTINGS_MAGIC) {
        const uint32_t v = p.getUInt("vol", 101u);
        if (v <= 100u) vol_pct = v;
    }
    p.end();
    (void)M5.Speaker.end();
    delay(10);
    (void)M5.Speaker.begin();
    const uint32_t v255 = (255u * vol_pct) / 100u;
    M5.Speaker.setVolume((uint8_t)v255);
    M5.Speaker.setAllChannelVolume((uint8_t)v255);
}

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
    audio_bootstrap();
    LvglHal::init();
    LvglHal::lock();
    app_init();
    LvglHal::unlock();
}

void loop() {
    M5.update();
    /* Defer OGSM radio startup until after splash audio init/dismiss.
     * Boot-time hosted Wi-Fi init can interfere with M5Unified speaker bring-up on Tab5. */
    static bool s_ogsm_started = false;
    if (!s_ogsm_started && s_splash_dismissed) {
        ogsm::service_init();
        s_ogsm_started = true;
    }
    if (s_ogsm_started) {
        ogsm::service_poll();
    }
    LvglHal::lock();
    lv_timer_handler();
    LvglHal::unlock();
    delay(5);
}
