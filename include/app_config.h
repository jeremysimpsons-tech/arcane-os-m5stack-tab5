/**
 * Single place for target geometry, insets, and the visual "Arcane" palette.
 * M5Stack Tab5: native DSI is 720×1280 in portrait; build uses APP_LCD_WIDTH/HEIGHT.
 */
#pragma once
#include <stdint.h>

/* Match platformio build_flags APP_LCD_WIDTH / APP_LCD_HEIGHT (portrait) */
#ifndef APP_LCD_WIDTH
    #define APP_LCD_WIDTH 720
#endif
#ifndef APP_LCD_HEIGHT
    #define APP_LCD_HEIGHT 1280
#endif

#define APP_TOPBAR_PX  144 /* doubled from 72; APP_CONTENT_H follows */
/* Reserved for Step 4 status row (Wi‑Fi + battery); not subtracted from APP_CONTENT_H until then. */
#define APP_STATUSBAR_PX 40
#define APP_SAFE_INSET_H 20
#define APP_SAFE_INSET_V 12

/* Dark app chrome (pre–iOS-style reskin) */
#define APP_C_BG 0x0D0D0E
#define APP_C_PANEL 0x1A1A1C
#define APP_C_PANEL2 0x1C1C1F
#define APP_C_BORDER 0x3A3A3C
#define APP_C_TEXT 0xF2F2F4
#define APP_C_TEXT_MUTE 0x9898A0
#define APP_C_ACCENT 0x007AFF
#define APP_C_TOP 0x000000 /* shell top bar: black */
#define APP_C_GRID_TILE 0x1E1E22

/* Light “sheet” + status tokens; in-app body (slate-grey with a cool hint). Home overrides to white in code. */
#define APP_C_HOME_SHEET 0xE4EBF0
#define APP_C_HOME_SCREEN 0xFFFFFF
#define APP_C_STATUSBAR_BG 0xECECF0
#define APP_C_SPLASH_BG 0x000000
/* Home springboard icon palette (grey family + one restrained blue accent). */
#define APP_C_ICON_GREY_DARK 0x5A6270
#define APP_C_ICON_GREY_MID 0x6F7A8A
#define APP_C_ICON_GREY_LIGHT 0x8E99AA
#define APP_C_ICON_ACCENT_BLUE 0x6DA7D8
/* Text / surfaces on light sheet (shell content body, Step 3). */
#define APP_C_SHEET_TEXT 0x1C1C1E
#define APP_C_SHEET_TEXT_MUTE 0x6E6E73
#define APP_C_SHEET_ROW 0xFFFFFF
#define APP_C_HOME_TILE 0x3A3A3C /* dark grey home launcher tiles */

/* Squircle / corner radii (LVGL px) */
#define APP_RADIUS_SQUIRCLE 26
#define APP_RADIUS_TILE 20
#define APP_RADIUS_SHEET 24

/*
 * Content height split (iOS-style stack):
 *   APP_CONTENT_H_SHELL — area below status + top bar (in-app shell pages).
 *   APP_CONTENT_H_SPRING — home springboard below status only (no extra top bar in that layout).
 * Today’s code still uses a single dark top bar and no status row, so keep the live macro unchanged.
 */
#define APP_CONTENT_H_SHELL (APP_LCD_HEIGHT - APP_STATUSBAR_PX - APP_TOPBAR_PX)
#define APP_CONTENT_H_SPRING (APP_LCD_HEIGHT - APP_STATUSBAR_PX)
#define APP_CONTENT_H (APP_LCD_HEIGHT - APP_TOPBAR_PX)
#define APP_CONTENT_W APP_LCD_WIDTH

#define APP_NVS_NAMESPACE "arc_os"
#define APP_NVS_CAL_MAGIC 0x7ABECAB1u

#define APP_NAME "ARCANE OS"
#define APP_NAME_SHORT "ARCANE"
#define APP_VERSION "0.1.0"
