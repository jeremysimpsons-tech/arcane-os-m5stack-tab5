# Arcane OS — M5Stack Tab5

Touch-first UI shell on **LVGL 9** (portrait **720×1280**, soft inertial scrolling, NVS touch calibration).

## Architecture (code map)

1. **`main.cpp`** — `M5.begin()` → `LvglHal::init()` (framebuffer + theme + indev + tick) → `app_init()`.
2. **`app_init()`** (`App.cpp`) — load touch cal from NVS; if missing, `view_calibrate()`, else `view_splash()`.
3. **Flow** — splash (no global chrome) → timer or tap → **Home** grid; other screens use **`shell_mount()`**: fixed top bar + **content** region only (no duplicate app header in content).
4. **Navigation** — `app_show(AppScreen)` switches the active `lv_screen` body; all transitions stay inside LVGL, under one mutex in the main loop.
5. **Config** — `include/app_config.h` (W×H, `APP_TOPBAR_PX`, colors, NVS key); `include/lv_conf.h` (LVGL build).

## Hardware notes

- **Panel** is native **portrait**; values are in `platformio.ini` as `APP_LCD_WIDTH/HEIGHT` and must match M5’s DSI (720×1280 for Tab5).
- **Touch** is mapped through stored min/max raw bounds after a 4-point pass; clear calibration from **Settings** (reboots).
- **Speaker** feedback on intentional taps: `LvglHal::click_feedback()` (disable with `-DDISABLE_SPK_FEEDBACK` if needed).
- **Known gaps**: camera / IMU / I2C / SD / WiFi stacks are **stubs** — wire the board-specific drivers and replace the placeholder pages.

## Build

`pio run -e tab5` then `pio run -e tab5 -t upload`
