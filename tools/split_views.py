#!/usr/bin/env python3
"""One-off: split src/Views.cpp into src/views/*.cpp (run from project root)."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "Views.cpp"
OUT = ROOT / "src" / "views"
OUT.mkdir(exist_ok=True)

lines = SRC.read_text(encoding="utf-8").splitlines(keepends=True)

HDR_STYLE = r'''#include "Views.hpp"
#include "AppTypes.hpp"
#include "views_config.hpp"
#include "views_internal.hpp"
#include "app_config.h"
#include "app_theme.hpp"
#include "arcane_lvgl.h"
#include <M5Unified.h>
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

'''

HDR_SHELL = r'''#include "Views.hpp"
#include "AppTypes.hpp"
#include "LvglHal.hpp"
#include "views_config.hpp"
#include "views_internal.hpp"
#include "app_config.h"
#include "app_theme.hpp"
#include "arcane_lvgl.h"
#include <M5Unified.h>
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

extern "C" const lv_img_dsc_t wolf;

'''

HDR_SPLASH = r'''#include "Views.hpp"
#include "AppTypes.hpp"
#include "views_config.hpp"
#include "views_internal.hpp"
#include "app_config.h"
#include "app_theme.hpp"
#include "arcane_lvgl.h"
#include <M5Unified.h>
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

extern "C" const lv_img_dsc_t wolf;

'''

HDR_CAL = r'''#include "Views.hpp"
#include "AppTypes.hpp"
#include "LvglHal.hpp"
#include "TouchCal.hpp"
#include "views_config.hpp"
#include "views_internal.hpp"
#include "app_config.h"
#include "app_theme.hpp"
#include "arcane_lvgl.h"
#include <M5Unified.h>
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <string.h>
#include "esp_system.h"

'''

HDR_HOME = r'''#include "Views.hpp"
#include "AppTypes.hpp"
#include "LvglHal.hpp"
#include "views_config.hpp"
#include "views_internal.hpp"
#include "app_config.h"
#include "app_theme.hpp"
#include "arcane_lvgl.h"
#include <M5Unified.h>
#include <Arduino.h>

'''

HDR_FILES = r'''#include "Views.hpp"
#include "AppTypes.hpp"
#include "LvglHal.hpp"
#include "views_config.hpp"
#include "views_internal.hpp"
#include "app_config.h"
#include "app_theme.hpp"
#include "arcane_lvgl.h"
#include <M5Unified.h>
#include <Arduino.h>

'''

HDR_TOOLS = r'''#include "Views.hpp"
#include "AppTypes.hpp"
#include "LvglHal.hpp"
#include "views_config.hpp"
#include "views_internal.hpp"
#include "app_config.h"
#include "app_theme.hpp"
#include "arcane_lvgl.h"
#include <M5Unified.h>
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

'''

HDR_SET = r'''#include "Views.hpp"
#include "AppTypes.hpp"
#include "LvglHal.hpp"
#include "TouchCal.hpp"
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
#include <string.h>
#include "esp_system.h"

'''

HDR_STAT = r'''#include "Views.hpp"
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

'''


def join_ranges(*ranges_1_inclusive: tuple[int, int]) -> str:
    out: list[str] = []
    for a, b in ranges_1_inclusive:
        out.extend(lines[a - 1 : b])
    return "".join(out)


def one(name: str, header: str, body: str, fixups: list[tuple[str, str]] | None = None):
    t = body
    for old, new in (fixups or []):
        t = t.replace(old, new)
    (OUT / name).write_text(header + "\n" + t, encoding="utf-8")


# style: hand-written UI helpers (no static theme blob)
style_body = r'''
void apply_scroll_tabled(lv_obj_t *o) {
    lv_obj_add_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_flag(o, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scrollbar_mode(o, LV_SCROLLBAR_MODE_AUTO);
}

void color_bg(lv_obj_t *o, uint32_t hex) {
    lv_obj_set_style_bg_color(o, lv_color_hex(hex), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
}

uint32_t ui_bg_content(void) {
    return s_ui_dark_mode ? 0x000000u : (uint32_t)APP_C_HOME_SCREEN;
}
uint32_t ui_text_primary(void) {
    return s_ui_dark_mode ? 0xF2F2F4u : (uint32_t)APP_C_SHEET_TEXT;
}
uint32_t ui_text_mute(void) {
    return s_ui_dark_mode ? 0x9898A0u : (uint32_t)APP_C_SHEET_TEXT_MUTE;
}
uint32_t ui_card_bg(void) {
    return s_ui_dark_mode ? 0x1C1C1Eu : 0xF2F2F7u;
}
uint32_t ui_card_border(void) {
    return s_ui_dark_mode ? 0x3A3A3Cu : 0xE5E5EAu;
}
uint32_t ui_cpu_chip_idle(void) {
    return s_ui_dark_mode ? 0x3A3A3Cu : 0xE8E8EDu;
}

void app_style_ios_slider(lv_obj_t *sl) {
    if (s_ui_dark_mode) {
        lv_obj_set_style_bg_color(sl, lv_color_hex(0x2C2C2E), LV_PART_MAIN);
        lv_obj_set_style_bg_color(sl, lv_color_hex(APP_C_ICON_ACCENT_BLUE), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(sl, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
        lv_obj_set_style_border_color(sl, lv_color_hex(0x636366), LV_PART_KNOB);
    } else {
        lv_obj_set_style_bg_color(sl, lv_color_hex(0xE5E5EA), LV_PART_MAIN);
        lv_obj_set_style_bg_color(sl, lv_color_hex(APP_C_ICON_ACCENT_BLUE), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(sl, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
        lv_obj_set_style_border_color(sl, lv_color_hex(0xD1D1D6), LV_PART_KNOB);
    }
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(sl, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(sl, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(sl, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_border_width(sl, 1, LV_PART_KNOB);
}

lv_obj_t *app_screen_title(lv_obj_t *c, const char *title) {
    lv_obj_t *t = lv_label_create(c);
    lv_label_set_text(t, title);
    lv_obj_set_width(t, lv_pct(100));
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(t, APP_FONT_HERO, LV_PART_MAIN);
    lv_obj_set_style_text_color(t, lv_color_hex(ui_text_primary()), LV_PART_MAIN);
    return t;
}

void app_style_muted_card(lv_obj_t *o) {
    color_bg(o, ui_card_bg());
    lv_obj_set_style_radius(o, 16, LV_PART_MAIN);
    lv_obj_set_style_border_color(o, lv_color_hex(ui_card_border()), LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(o, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(o, 0, LV_PART_MAIN);
}
'''
one("views_style.cpp", HDR_STYLE, style_body)

# shell: 305-683 (was static content_ptr, s_cbody line 303 removed in source extract)
sh = join_ranges((305, 683))
sh = sh.replace("static lv_obj_t *content_ptr()", "lv_obj_t *content_ptr()")
# s_cbody was at 303 — not in this chunk, good
one("views_shell.cpp", HDR_SHELL, sh)

one("views_splash.cpp", HDR_SPLASH, join_ranges((70, 198)))
one("views_calibrate.cpp", HDR_CAL, join_ranges((201, 300)))
one("views_home.cpp", HDR_HOME, join_ranges((722, 836)))

# files: 838-895, rename noop
fb = join_ranges((838, 895))
fb = fb.replace("static void noop_right", "void views_noop_right")
one("views_files.cpp", HDR_FILES, fb)

# Wi-Fi app removed from project (was 901-1476 in legacy monolith).

# tools 1477-1636 i2c uses noop - 1611 is view_i2c; 1507 view_power
tb = join_ranges((1477, 1638))
tb = tb.replace("noop_right", "views_noop_right")
one("views_tools.cpp", HDR_TOOLS, tb)
# add power_off at end of tools file - it was 325-331 in original inside shell! **power_off is in shell chunk** 325-330
# check - view_power_menu uses power_off_do_cb - defined line 325 in original shell section. It IS in 305-683. Good.

# settings 1640-2210
set1 = join_ranges((1640, 2210))
# remove enum SettingsCat and static s_settings if duplicated - s_settings in state, enums in header
set1 = re.sub(
    r"enum class SettingsCat : uint8_t \{[^}]+\};\n\n",
    "",
    set1,
    count=1,
)
set1 = set1.replace("static SettingsCat s_settings_cat = SettingsCat::Power;\n", "")
# remove duplicate statics now in state:
for pat in [
    "static int         s_cpu_mhz",
    "static int         s_lora_mix",
    "static bool        s_mesh_gateway",
    "static int         s_bl_timeout_s",
    "static int         s_dim_timeout_s",
    "static uint8_t     s_volume_pct",
    "static bool       s_settings_dirty",
    "static lv_obj_t * s_settings_save_top",
    "static lv_obj_t * s_settings_save_bottom",
]:
    set1 = set1.replace(pat, pat.replace("static ", ""), 1)

# fix remaining static for cat - we removed s_settings_cat line; need s_settings_cat without static - it is extern, remove assignment line entirely - we have in state: s_settings_cat = Power. The original has "static SettingsCat s_settings_cat = ..." - removed. Good.

one("views_settings.cpp", HDR_SET, set1)

# status 2212-2472
st = join_ranges((2212, 2472))
st = re.sub(
    r"enum class StatusCat : uint8_t \{[^}]+\};\nstatic StatusCat s_status_cat[^\n]*\n\n",
    "",
    st,
    count=1,
)
one("views_status.cpp", HDR_STAT, st)

# view_settings 2473-2605 append to settings file
vset = join_ranges((2473, 2605))
(OUT / "views_settings.cpp").open("a", encoding="utf-8").write("\n" + vset)

# arc_settings: in original 1661 - part of set1. Good.

# arc_notify, arc_settings_load in set1. Good.

print("Wrote", OUT, "and views_style, shell, ...")
