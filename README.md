# Arcane OS — M5Stack Tab5

Touch-first “shell” UI for the **M5Stack Tab5** (ESP32-P4) using **LVGL 9**, **M5Unified**, and **M5GFX**. The firmware presents a full-screen app launcher, a shared top bar, and a set of system/apps screens. This repository is a **self-contained** PlatformIO project: build with PlatformIO, flash over USB, and use the on-device capacitive display.

**Display profile:** portrait `720×1280` (see `include/app_config.h` and `platformio.ini` `APP_LCD_WIDTH` / `APP_LCD_HEIGHT`).

## Features (implemented)

### Boot & navigation

- **Splash** with wolf asset, “ARCANE OS” title, “Home Base Edition” subtitle, fade-in, and **tap to continue** to the home screen.
- **Back stack:** `app_show()` routes to screens defined in `src/App.cpp` (Home, Settings, Files, etc.).

### Home screen

- **iPhone-style grid:** **4 tiles per row**, rounded **blue accent** tiles, white **LVGL symbol** icons, labels under each tile.
- **Vertical** finger scrolling with inertial / elastic behavior (lists use the same scroll helpers where applied).

### Shell (top bar)

- Dark bar with **“ARCANE OS”** center title on home; in apps, **home/back** action on the left (home icon can be **scaled** for legibility).
- **Battery** percentage with **color ramp** (low → mid → good).
- **Charging** bolt shown **to the left** of the battery icon **only while charging**; the battery symbol continues to show **level** while charging.
- **Power** control access as implemented in the shell (power menu flow).

### Settings

- **Sidebar + content** layout (suited to Tab5’s larger screen): categories **Power**, **Comms**, **Security**, **Sensors** (icon column + detail panel; panel scrolls).
- **Backlight timeout:** off / **5s** / **10s** / **30s** of inactivity → backlight off; any touch restores (based on a lightweight idle check tied to the active screen).
- **CPU frequency:** **80 / 160 / 240 MHz** (Arduino `setCpuFrequencyMhz`).

**UI-only (placeholders, no radio/mesh/RTC driver wired yet):**

- LoRa spreading factor slider (range vs speed copy).
- Mesh gateway (repeater) switch.
- RTC sync buttons (manual / GPS / internet) as informational placeholders.

### Other apps (scaffold / demo)

- **Files:** storage selector row (Internal / SD) and a demo file list; real filesystem integration is left for later work.
- **Camera, IMU, Power, SD, touch test, I2C, RTC** screens are present as lightweight placeholders or small demos, consistent with the shared shell.

### Build notes

- **PSRAM** is assumed available (`-DBOARD_HAS_PSRAM` in `platformio.ini`).
- A **pre-build script** `patch_m5gfx_p4.py` adjusts the LVGL/M5GFX integration for this target.
- Default flash usage is reported by PlatformIO after link; the factory partition layout targets a **1.25 MiB** app slot—keep an eye on size if you add large assets or many fonts in `include/lv_conf.h`.

## Repository layout (important paths)

| Path | Role |
|------|------|
| `platformio.ini` | Environment `tab5`, ESP32-P4, LVGL, M5 deps |
| `src/main.cpp` | Arduino `setup` / `loop`, M5 + LVGL tick |
| `src/App.cpp` | `app_init`, `app_show` screen router |
| `src/Views.cpp` | Shell, home, settings, and app UIs |
| `src/LvglHal.cpp` | Display/touch + LVGL port glue |
| `include/lv_conf.h` | LVGL configuration |
| `include/app_config.h` | Layout sizes and palette |
| `include/wolf.c` + `src/wolf_asset.c` | Splash wolf image data |

## Build and flash

```bash
cd arcane-os-m5stack-tab5
pio run -e tab5
```

Upload (with the device connected):

```bash
pio run -e tab5 -t upload
```

Serial monitor (match `monitor_speed` in `platformio.ini`, typically `115200`):

```bash
pio device monitor -b 115200
```

## License / credits

- **M5Stack** hardware, **M5Unified** / **M5GFX** libraries, **LVGL** — see their respective licenses in PlatformIO’s `libdeps` and upstream repos.
- **Arcane OS** is a custom shell UI built on top of these components.

## Contributing

This tree is meant to grow incrementally: prefer small, testable changes on real hardware, especially around touch, display rotation, and flash size when enabling extra LVGL fonts or assets.
