#include "Views.hpp"
#include "AppTypes.hpp"
#include "LvglHal.hpp"
#include "views_config.hpp"
#include "views_internal.hpp"
#include "app_config.h"
#include "app_theme.hpp"
#include "arcane_lvgl.h"
#include <M5Unified.h>
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include <stdio.h>

extern "C" const lv_img_dsc_t wolf;

/*
 * `include/startup.wav` — `board_build.embed_files` in platformio.ini
 *
 * Factory Tab5 (ESP-IDF) firmware uses the same hardware path from a different stack:
 *   m5stack/M5Tab5-UserDemo — `bsp_audio_codec_speaker_init()`, ES8388, amp via PI4IOE.
 * Here, M5Unified sets Tab5 I2S + `_speaker_enabled_cb_tab5` (ES8388 + `In_I2C` amp) on first
 * `tone` / `playWav` / `playRaw`. If `playWav` fails, re-export the WAV as standard PCM
 * (some tools insert chunks M5’s parser does not like).
 */
extern "C" const uint8_t _binary_include_startup_wav_start[] __attribute__((aligned(4)));
extern "C" const uint8_t _binary_include_startup_wav_end[];

/* Master from NVS/settings before splash boost; `splash_dismiss_cb` restores for Home/shell. */
static uint8_t s_splash_spk_volume_restore = 255u;

static void wav_fmt_from_embed(const uint8_t *f, size_t n, uint16_t *out_ch, uint32_t *out_rate, uint16_t *out_bits) {
    *out_ch  = 0u;
    *out_rate = 0u;
    *out_bits = 0u;
    if (n < 12u || memcmp(f, "RIFF", 4) != 0u || memcmp(f + 8, "WAVE", 4) != 0u)
        return;
    auto rd16 = [](const uint8_t *b) -> uint16_t { return (uint16_t)(b[0] | (b[1] << 8)); };
    auto rd32 = [](const uint8_t *b) -> uint32_t {
        return (uint32_t)(b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24));
    };
    for (size_t o = 12u; o + 8u <= n;) {
        const uint32_t csize = rd32(f + o + 4u);
        if (memcmp(f + o, "fmt ", 4) == 0 && csize >= 16u) {
            *out_ch   = rd16(f + o + 8u + 2u);
            *out_rate  = rd32(f + o + 8u + 4u);
            *out_bits  = rd16(f + o + 8u + 14u);
            return;
        }
        o += 8u + csize + (csize & 1u);
    }
}

/* Call immediately before `splash_fade_in(mark, …)` so the WAV and logo fade start together. */
static void splash_play_startup_wav() {
    const uint8_t *p = _binary_include_startup_wav_start;
    const size_t   n = (size_t)(_binary_include_startup_wav_end - _binary_include_startup_wav_start);
    uint16_t       wch = 0u, wbits = 0u;
    uint32_t       wrate  = 0u;
    wav_fmt_from_embed(p, n, &wch, &wrate, &wbits);

#if defined(M5STACK_TAB5) || defined(CONFIG_IDF_TARGET_ESP32P4)
    /* M5 default for Tab5: stereo=0, mag=4. Stereo WAV + low mag / mono I2S can be inaudible; align before first begin(). */
    {
        m5::speaker_config_t sc = M5.Speaker.config();
        sc.stereo = (wch >= 2u);
        sc.magnification = (wch >= 2u) ? 32u : 16u;
        M5.Speaker.config(sc);
    }
#endif

    s_splash_spk_volume_restore = M5.Speaker.getVolume();
    uint32_t sp = (uint32_t)APP_SPLASH_VOLUME;
    if (sp > 255u)
        sp = 255u;
    const uint8_t splash_master = (uint8_t)sp;

#if ARC_DEBUG_SPLASH_AUDIO
    {
        const uint8_t  master_before = M5.Speaker.getVolume();
        const uint8_t  ch0           = M5.Speaker.getChannelVolume(0);
        uint32_t       nvs_vol      = 101u;
        const m5::board_t br        = M5.getBoard();
        (void)M5.Speaker.isEnabled();
        (void)br;
        Preferences pref;
        if (pref.begin(ARC_SETTINGS_NVS, true) && pref.getUInt("magic", 0) == ARC_SETTINGS_MAGIC) {
            nvs_vol = pref.getUInt("vol", 101u);
        }
        pref.end();
        const auto spk = M5.Speaker.config();
        Serial.println();
        Serial.println("--- arc: splash audio ---");
        Serial.printf("  board (enum)=%d  (M5Tab5=%d)  spk isEnabled=%d  isRunning=%d\n", (int)br,
            (int)m5::board_t::board_M5Tab5, (int)M5.Speaker.isEnabled(), (int)M5.Speaker.isRunning());
        Serial.printf("  M5.Speaker: master_now=%u ch0=%u  (NVS vol%% key=%u, 101=unset)  splash setVolume=%u\n",
            (unsigned)master_before, (unsigned)ch0, (unsigned)nvs_vol, (unsigned)splash_master);
        Serial.printf("  spk config: data_out=%d bck=%d ws=%d mck=%d  i2s=%d  mag=%u\n", spk.pin_data_out,
            spk.pin_bck, spk.pin_ws, spk.pin_mck, (int)spk.i2s_port, (unsigned)spk.magnification);
        Serial.printf("  startup.wav embed: %u bytes, head=%.4s\n", (unsigned)n, (char *)p);
        if (wch > 0u && wrate > 0u) {
            Serial.printf("  wav fmt: %u Hz, %u ch, %u bit/sample (PCM)\n", (unsigned)wrate, (unsigned)wch,
                (unsigned)wbits);
        } else {
            Serial.println("  wav fmt: (fmt chunk not found — odd file)");
        }
        if (n < 12u || memcmp(p, "RIFF", 4) != 0) {
            Serial.println("  ERROR: embed not a RIFF / too small");
        }
    }
#endif

    const bool began = M5.Speaker.begin();
    M5.Speaker.setVolume(splash_master);
    M5.Speaker.setAllChannelVolume(splash_master);
    delay(40);

#if ARC_DEBUG_SPLASH_AUDIO
    Serial.printf("  Speaker.begin()=%d  after setVolume: master=%u\n", (int)began, (unsigned)M5.Speaker.getVolume());
#endif

#if ARC_SPLASH_SMOKE_TONE
    (void)M5.Speaker.tone(1000.0f, 60u, -1, true);
    delay(70);
# if ARC_DEBUG_SPLASH_AUDIO
    Serial.printf("  after smoke tone: isPlaying=%d ch_playing=%u\n", (int)M5.Speaker.isPlaying(),
        (unsigned)M5.Speaker.getPlayingChannels());
# endif
#endif

    const bool ok = M5.Speaker.playWav(p, n, 1, -1, true);
#if ARC_DEBUG_SPLASH_AUDIO
    delay(10);
    Serial.printf("  playWav() => %d   isPlaying=%d  ch_playing=%u  (logo fade next)\n", (int)ok,
        (int)M5.Speaker.isPlaying(), (unsigned)M5.Speaker.getPlayingChannels());
    if (!ok) {
        Serial.println("  (playWav false: not PCM, bad header, or data chunk not found — re-export 16-bit PCM WAV)");
    }
    Serial.println("--- end splash audio ---");
    Serial.println();
    Serial.flush();
#endif
    (void)ok;
    (void)began;
}


/* ---------- full-screen chrome-free splash (dismiss on touch only; no timer) ---------- */
/* s_splash_dismissed: views_state */

/** Splash content fade duration (ms). */
static constexpr uint32_t SPLASH_FADE_MS = 1800u;
/** Startup bar shuttle: one full left→right→left cycle (slower than fade; loops until splash dismissed). */
static constexpr uint32_t SPLASH_BAR_CYCLE_MS = 3200u;

static void splash_boot_bar_anim(lv_obj_t *seg, lv_coord_t x_min, lv_coord_t x_max) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, seg);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&a, x_min, x_max);
    lv_anim_set_duration(&a, SPLASH_BAR_CYCLE_MS / 2u);
    lv_anim_set_playback_time(&a, SPLASH_BAR_CYCLE_MS / 2u);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void splash_dismiss_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_PRESSED)
        return;
    if (s_splash_dismissed)
        return;
    s_splash_dismissed = true;
    M5.Speaker.setVolume(s_splash_spk_volume_restore);
    M5.Speaker.setAllChannelVolume(255u);
    LvglHal::click_feedback();
    app_show(AppScreen::Home);
}

static void splash_add_dismiss(lv_obj_t *o) {
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(o, splash_dismiss_cb, LV_EVENT_PRESSED, nullptr);
}

static void splash_fade_in(lv_obj_t *o, uint32_t dur_ms) {
    if (!o || !lv_obj_is_valid(o))
        return;
    lv_obj_set_style_opa(o, LV_OPA_0, LV_PART_MAIN);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_values(&a, LV_OPA_0, LV_OPA_COVER);
    lv_anim_set_duration(&a, dur_ms);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)[](void *obj, int32_t v) {
        lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, LV_PART_MAIN);
    });
    lv_anim_start(&a);
}

void view_splash() {
    s_splash_dismissed = false;
    lv_obj_t *scr = lv_screen_active();
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clean(scr);
    color_bg(scr, APP_C_SPLASH_BG);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);

    /* Centered splash mark: wolf image + ARCANE OS title + subtitle */
    lv_obj_t *mark = lv_obj_create(scr);
    lv_obj_set_size(mark, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(mark, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(mark, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(mark, 0, LV_PART_MAIN);
    lv_obj_set_layout(mark, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(mark, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mark, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(mark, 18, LV_PART_MAIN);
    lv_obj_center(mark);
    lv_obj_set_y(mark, lv_obj_get_y(mark) - 80);

    lv_obj_t *wim = lv_image_create(mark);
    lv_image_set_src(wim, &wolf);

    lv_obj_t *title = lv_label_create(mark);
    lv_label_set_text(title, APP_NAME);
    lv_obj_set_style_text_font(title, APP_FONT_HERO, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    lv_obj_t *ver = lv_label_create(mark);
    lv_label_set_text(ver, "Home Base Edition");
    lv_obj_set_style_text_font(ver, APP_FONT_SUB, LV_PART_MAIN);
    lv_obj_set_style_text_color(ver, lv_color_hex(0xA8A8AE), LV_PART_MAIN);

    /* macOS-style startup bar: pill track + lighter segment shuttling with the fade. */
    constexpr lv_coord_t bar_track_w = 300;
    constexpr lv_coord_t bar_track_h = 6;
    constexpr lv_coord_t bar_seg_w   = 88;
    constexpr lv_coord_t bar_seg_h   = 4;
    constexpr lv_coord_t bar_pad_x   = 3;

    lv_obj_t *bar_track = lv_obj_create(mark);
    lv_obj_set_size(bar_track, bar_track_w, bar_track_h);
    lv_obj_set_style_bg_opa(bar_track, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_track, lv_color_hex(0x3A3A3C), LV_PART_MAIN);
    lv_obj_set_style_radius(bar_track, bar_track_h / 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_track, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar_track, 0, LV_PART_MAIN);
    lv_obj_remove_flag(bar_track, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bar_seg = lv_obj_create(bar_track);
    lv_obj_set_size(bar_seg, bar_seg_w, bar_seg_h);
    lv_obj_set_pos(bar_seg, bar_pad_x, (bar_track_h - bar_seg_h) / 2);
    lv_obj_set_style_bg_opa(bar_seg, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_seg, lv_color_hex(0xD1D1D6), LV_PART_MAIN);
    lv_obj_set_style_radius(bar_seg, (bar_seg_h + 1) / 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_seg, 0, LV_PART_MAIN);
    lv_obj_remove_flag(bar_seg, LV_OBJ_FLAG_SCROLLABLE);

    splash_boot_bar_anim(bar_seg, bar_pad_x, bar_track_w - bar_seg_w - bar_pad_x);

    /* Hide mark until fade (avoid visible logo during blocking speaker + playWav). */
    lv_obj_set_style_opa(mark, LV_OPA_0, LV_PART_MAIN);
    /* Jingle then fade-in: same `view_splash` call; first `lv_timer_handler` advances both. */
    splash_play_startup_wav();
    splash_fade_in(mark, SPLASH_FADE_MS);

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "Touch screen to continue");
    lv_obj_set_style_text_font(hint, APP_FONT_HEADING, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(APP_C_ACCENT), LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -48);

    splash_add_dismiss(scr);
    splash_add_dismiss(mark);
    splash_add_dismiss(wim);
    splash_add_dismiss(title);
    splash_add_dismiss(ver);
    splash_add_dismiss(bar_track);
    splash_add_dismiss(bar_seg);
    splash_add_dismiss(hint);
}
