#include "Views.hpp"
#include "AppTypes.hpp"
#include "views_config.hpp"
#include "views_internal.hpp"
#include "app_config.h"
#include "app_theme.hpp"
#include "arcane_lvgl.h"
#include <M5Unified.h>
#include <Arduino.h>
#include <Preferences.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <esp_chip_info.h>
#include "esp32-hal.h"
#if __has_include(<SD_MMC.h>)
#    include <SD_MMC.h>
#endif
#ifndef ARC_HAVE_SD_MMC
#    if __has_include(<SD_MMC.h>)
#        define ARC_HAVE_SD_MMC 1
#    else
#        define ARC_HAVE_SD_MMC 0
#    endif
#endif

extern int  s_cpu_mhz;
extern void settings_sidebar_btn_style(lv_obj_t *b, bool active);
extern void settings_content_title(lv_obj_t *parent, const char *t);


static void status_add_card(lv_obj_t *panel, const char *title, const char *body) {
    settings_content_title(panel, title);
    lv_obj_t *box = lv_obj_create(panel);
    lv_obj_set_width(box, lv_pct(100));
    app_style_muted_card(box);
    lv_obj_t *t = lv_label_create(box);
    lv_label_set_text(t, body);
    lv_obj_set_width(t, lv_pct(100));
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_font(t, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(t, lv_color_hex(ui_text_primary()), LV_PART_MAIN);
}

static void status_build_content(lv_obj_t *panel) {
    lv_obj_clean(panel);
    lv_obj_set_style_pad_row(panel, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 12, LV_PART_MAIN);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);
    apply_scroll_tabled(panel);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    M5.update();

    if (s_status_cat == StatusCat::System) {
        settings_content_title(panel, "System");
        esp_chip_info_t ci {};
        esp_chip_info(&ci);
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "Chip model: %s\n"
                 "Chip revision: %u\n"
                 "Cores: %u\n"
                 "CPU frequency: %u MHz\n"
                 "Settings CPU policy: %d MHz\n"
                 "SDK: %s\n"
                 "Features:%s%s%s%s%s",
                 ESP.getChipModel(), (unsigned)ESP.getChipRevision(), (unsigned)ESP.getChipCores(),
                 (unsigned)getCpuFrequencyMhz(), s_cpu_mhz, ESP.getSdkVersion(),
                 (ci.features & CHIP_FEATURE_WIFI_BGN) ? " Wi-Fi" : "",
                 (ci.features & CHIP_FEATURE_EMB_PSRAM) ? " PSRAM" : "",
                 (ci.features & CHIP_FEATURE_EMB_FLASH) ? " emb-flash" : "",
                 (ci.features & CHIP_FEATURE_BLE) ? " BLE" : "",
                 (ci.features & CHIP_FEATURE_BT) ? " BT" : "");
        status_add_card(panel, "Device", buf);
    } else if (s_status_cat == StatusCat::StorageRam) {
        settings_content_title(panel, "Storage and RAM");
        const uint32_t freeRAM   = ESP.getFreeHeap();
        const uint32_t minHeap   = ESP.getMinFreeHeap();
        const uint32_t usedFlash = ESP.getSketchSize();
        const uint32_t freeFlash = ESP.getFreeSketchSpace();
        const uint32_t totalFlash = ESP.getFlashChipSize();
        const uint32_t psramSz   = ESP.getPsramSize();
        const uint32_t freePsram = ESP.getFreePsram();
        const uint32_t minPsram  = ESP.getMinFreePsram();

        char buf[384];
        snprintf(buf, sizeof(buf),
                 "Internal RAM free: %lu B\n"
                 "Internal RAM min free: %lu B\n"
                 "PSRAM size: %lu B\n"
                 "PSRAM free: %lu B\n"
                 "PSRAM min free: %lu B\n"
                 "Sketch used: %lu B\n"
                 "Sketch free: %lu B\n"
                 "Flash chip size: %lu B",
                 (unsigned long)freeRAM, (unsigned long)minHeap, (unsigned long)psramSz, (unsigned long)freePsram,
                 (unsigned long)minPsram, (unsigned long)usedFlash, (unsigned long)freeFlash,
                 (unsigned long)totalFlash);
        status_add_card(panel, "Memory and firmware", buf);

        char sdb[384];
#if ARC_HAVE_SD_MMC
        if (SD_MMC.cardType() != CARD_NONE) {
            const uint64_t tot = SD_MMC.totalBytes();
            const uint64_t use = SD_MMC.usedBytes();
            const uint64_t fr  = (tot > use) ? (tot - use) : 0ull;
            const unsigned pcf = (tot > 0ull) ? (unsigned)((100ull * fr) / tot) : 0u;
            snprintf(sdb, sizeof(sdb),
                     "SD total: %.2f GiB\nSD used: %.2f GiB\nSD free: %u%% (~%.2f GiB)",
                     (double)tot / (1024.0 * 1024.0 * 1024.0), (double)use / (1024.0 * 1024.0 * 1024.0), pcf,
                     (double)fr / (1024.0 * 1024.0 * 1024.0));
        } else
#endif
        {
            snprintf(sdb, sizeof(sdb),
                     "No SD volume reported (not mounted or no card).\n"
                     "If your Tab5 build exposes SD_MMC, mount it in firmware to populate this block.");
        }
        status_add_card(panel, "SD card", sdb);
    } else if (s_status_cat == StatusCat::Power) {
        settings_content_title(panel, "Power");
        const int bat_mV = (int)M5.Power.getBatteryVoltage();
        const int vbus_mV = (int)M5.Power.getVBUSVoltage();
        int         pct    = (int)M5.Power.getBatteryLevel();
        const m5::Power_Class::is_charging_t chg = M5.Power.isCharging();
        bool        charging = (chg == m5::Power_Class::is_charging);
        if (!charging && chg == m5::Power_Class::charge_unknown && vbus_mV > 4200)
            charging = true;
        char buf[320];
        snprintf(buf, sizeof(buf),
                 "Battery voltage: %d mV\n"
                 "VBUS voltage: %d mV\n"
                 "Battery level (driver): %d%%\n"
                 "Charging: %s",
                 bat_mV, vbus_mV, pct, charging ? "yes" : "no");
        status_add_card(panel, "Rails", buf);
    } else {
        settings_content_title(panel, "Other");
        float tempC = temperatureRead();
        char  tbuf[96];
        if (tempC > -55.0f && tempC < 125.0f)
            snprintf(tbuf, sizeof(tbuf), "Chip temperature (approx.): %.1f C", (double)tempC);
        else
            snprintf(tbuf, sizeof(tbuf), "Chip temperature: not available on this build.");

        const uint64_t mac = ESP.getEfuseMac();
        char           mbuf[32];
        snprintf(mbuf, sizeof(mbuf), "%04X%08X", (unsigned)((uint32_t)(mac >> 32) & 0xFFFFu), (unsigned)(mac & 0xFFFFFFFFu));

        const time_t now = time(nullptr);
        const bool   plausible = (now > (time_t)1700000000);
        char         clk[256];
        if (!plausible)
            snprintf(clk, sizeof(clk),
                     "Not synchronized.\n"
                     "Source: — (set time manually or enable NTP when Wi-Fi is available.)");
        else
            snprintf(clk, sizeof(clk),
                     "Clock appears set (after ~Nov 2023).\n"
                     "Source: not tracked yet — extend with SNTP/RTC to show NTP vs manual.");

        char body[640];
        snprintf(body, sizeof(body),
                 "%s\n\n"
                 "EFUSE MAC: %s\n\n"
                 "Time synchronisation\n%s\n\n"
                 "Connected devices\n"
                 "None detected.\n"
                 "(Stack units such as GPS, ENV sensors, or Keyboard Unit v2 need a discovery pass — not wired yet.)",
                 tbuf, mbuf, clk);
        status_add_card(panel, "Sensors and clock", body);
    }
}

void view_status() {
    shell_mount("Status", nullptr, nullptr, false);
    lv_obj_t *c = content_ptr();

    lv_obj_set_layout(c, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_column(c, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(c, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(c, 0, LV_PART_MAIN);

    /* "Status" is only in the top bar. */
    lv_obj_t *body = lv_obj_create(c);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(body, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(body, 0, LV_PART_MAIN);
    lv_obj_set_layout(body, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(body, 14, LV_PART_MAIN);

    lv_obj_t *sidebar = lv_obj_create(body);
    lv_obj_set_size(sidebar, 212, lv_pct(100));
    lv_obj_set_style_bg_opa(sidebar, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(sidebar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(sidebar, 0, LV_PART_MAIN);
    lv_obj_set_layout(sidebar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(sidebar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sidebar, 10, LV_PART_MAIN);
    lv_obj_remove_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(body);
    lv_obj_set_flex_grow(panel, 1);
    lv_obj_set_height(panel, lv_pct(100));
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);

    auto mk_side = [&](const char *sym, const char *caption, StatusCat cat) {
        lv_obj_t *b = lv_button_create(sidebar);
        lv_obj_set_width(b, lv_pct(100));
        lv_obj_set_height(b, LV_SIZE_CONTENT);
        settings_sidebar_btn_style(b, s_status_cat == cat);
        lv_obj_set_layout(b, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_min_height(b, 108, LV_PART_MAIN);
        lv_obj_set_style_pad_ver(b, 18, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(b, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_row(b, 6, LV_PART_MAIN);

        lv_obj_t *ic = lv_label_create(b);
        lv_label_set_text(ic, sym);
        lv_obj_set_style_text_font(ic, APP_FONT_HERO, LV_PART_MAIN);
        lv_obj_set_style_text_color(ic, lv_color_hex(APP_C_ICON_ACCENT_BLUE), LV_PART_MAIN);
        lv_obj_set_style_text_align(ic, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

        lv_obj_t *cap = lv_label_create(b);
        lv_label_set_text(cap, caption);
        lv_obj_set_style_text_font(cap, APP_FONT_BODY, LV_PART_MAIN);
        lv_obj_set_style_text_color(cap, lv_color_hex(ui_text_mute()), LV_PART_MAIN);
        lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_width(cap, lv_pct(100));
        lv_label_set_long_mode(cap, LV_LABEL_LONG_MODE_DOTS);

        lv_obj_add_event_cb(
            b,
            [](lv_event_t *e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED)
                    return;
                s_status_cat = (StatusCat)(uintptr_t)lv_event_get_user_data(e);
                lv_obj_t *sb = lv_obj_get_parent((lv_obj_t *)lv_event_get_target(e));
                const uint32_t nch = lv_obj_get_child_count(sb);
                for (uint32_t i = 0; i < nch; ++i) {
                    lv_obj_t *ch = lv_obj_get_child(sb, i);
                    if (!ch)
                        continue;
                    StatusCat ccat = (StatusCat)(uintptr_t)lv_obj_get_user_data(ch);
                    settings_sidebar_btn_style(ch, ccat == s_status_cat);
                }
                lv_obj_t *root = lv_obj_get_parent(sb);
                lv_obj_t *pn   = lv_obj_get_child(root, 1);
                status_build_content(pn);
            },
            LV_EVENT_CLICKED, (void *)(uintptr_t)cat);
        lv_obj_set_user_data(b, (void *)(uintptr_t)cat);
        return b;
    };

    mk_side(LV_SYMBOL_SETTINGS, "System", StatusCat::System);
    mk_side(LV_SYMBOL_DRIVE, "Storage\nand RAM", StatusCat::StorageRam);
    mk_side(LV_SYMBOL_CHARGE, "Power", StatusCat::Power);
    mk_side(LV_SYMBOL_LIST, "Other", StatusCat::Other);

    status_build_content(panel);
}

