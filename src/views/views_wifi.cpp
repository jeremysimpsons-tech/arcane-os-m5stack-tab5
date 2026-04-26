#include "Views.hpp"
#include "AppTypes.hpp"
#include "LvglHal.hpp"
#include "Tab5M5Comms.hpp"
#include "arc_wifi.hpp"
#include "views_config.hpp"
#include "views_internal.hpp"
#include "app_config.h"
#include "app_theme.hpp"
#include "arcane_lvgl.h"
#include <M5Unified.h>
#include <Arduino.h>
#include <esp_wifi_types.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <atomic>
#include <algorithm>
#include <string.h>
#include <string>
#include <vector>
#include <WiFiType.h>

struct WifiApRow {
    char    ssid[33];
    int32_t rssi;
    uint8_t channel;
    int     enc;
};

struct ConnJob {
    char  ssid[33];
    char  pass[65];
    bool  is_open;
};

/** In-flight connect: `beginConnect` + poll run on LVGL/loop (not a FreeRTOS task — `WiFi.begin` can reset Tab5 C6/SDIO if called from wconn). */
struct ConnSt {
    ConnJob j;
    int     tick; /* increments each timer pass after begin; fail when reaches 79 (80th check) */
};

static lv_timer_t *s_connect_timer;
/** Set while association is in progress — blocks new scans to avoid C6/SDIO races with the scan task. */
static std::atomic<bool> s_wifi_connecting{false};

struct RowU {
    char full_ssid[33];
    int  enc;
};

struct PwPack {
    lv_obj_t   *root;
    lv_obj_t   *ta;
    /** Full-bleed area above keyboard: tap outside the card dismisses. */
    lv_obj_t   *dismiss_area;
    char        ssid[33];
};

static const char   kmsg_ok[]  = "Connected. Network saved for next boot.";
static const char   kmsg_fail[] = "Could not connect. Check the password and try again.";

static SemaphoreHandle_t         s_aps_mutex;
static std::vector<WifiApRow>    s_aps;
static char                      s_scan_err[96];
static volatile bool             s_scan_done;
static volatile bool             s_scan_running;
/** True after opening the screen until the user runs Scan (avoids auto `scanNetworks` on entry). */
static bool                      s_tap_to_scan_hint = true;
static std::string               s_toast;
static const char *              s_on_conn_info;

static lv_timer_t *s_poll;
static lv_obj_t   *s_list;
static lv_obj_t   *s_status;
static void (*s_rebuild)();
static void        wifi_rebuild();

#if ARC_DEBUG_WIFI
static void wifi_dbg(const char *line) {
    Serial.print("[arc:wifi] ");
    Serial.println(line);
    Serial.flush();
}
static void wifi_dbgf(const char *fmt, ...) {
    Serial.print("[arc:wifi] ");
    char    buf[192];
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    Serial.println(buf);
    Serial.flush();
}
#else
static void wifi_dbg(const char *) {}
static void wifi_dbgf(const char *, ...) {}
#endif

/* ---------- top status (centered, large; mirrors header state only) ---------- */
static void status_update() {
    if (!s_status || !lv_obj_is_valid(s_status)) {
        return;
    }
#if ARC_DEBUG_WIFI
    static uint32_t s_st_n;
    s_st_n++;
    if (s_st_n <= 10u || (s_st_n % 80u) == 0u) {
        wifi_dbgf("status_update #%u", (unsigned)s_st_n);
    }
#endif
    if (!s_toast.empty()) {
        lv_label_set_text(s_status, s_toast.c_str());
        lv_obj_set_style_text_color(s_status, lv_color_hex(APP_C_ICON_ACCENT_BLUE), LV_PART_MAIN);
        lv_obj_set_style_text_font(s_status, APP_FONT_SUB, LV_PART_MAIN);
        s_toast.clear();
        return;
    }
    if (!M5Comms.isReady()) {
#if ARC_DEBUG_WIFI
        if (s_st_n <= 10u) {
            wifi_dbg("status: branch not_ready (isReady=0)");
        }
#endif
        lv_label_set_text(s_status, "Wi-Fi module not ready (SDIO / C6).");
        lv_obj_set_style_text_color(s_status, lv_color_hex(ui_text_mute()), LV_PART_MAIN);
        lv_obj_set_style_text_font(s_status, APP_FONT_SUB, LV_PART_MAIN);
        return;
    }
    if ((int)M5Comms.WiFi.status() == WL_CONNECTED) {
        arc_net_poll();
#if ARC_DEBUG_WIFI
        if (s_st_n <= 12u) {
            wifi_dbg("status: branch connected (after arc_net_poll)");
        }
#endif
        char        b[200];
        String      ip = M5Comms.WiFi.localIPString();
        int         r  = M5Comms.WiFi.connectedRSSI();
        const char *S  = M5Comms.WiFi.connectedSSID().c_str();
        const bool  ok = arc_net_is_online();
        (void)snprintf(
            b, sizeof b, "Connected: %s  -  %d dBm  -  %s  -  %s", S, r, ip.c_str(), ok ? "Internet" : "No internet");
        lv_label_set_text(s_status, b);
        lv_obj_set_style_text_font(s_status, APP_FONT_BODY, LV_PART_MAIN);
    } else {
        lv_label_set_text(s_status, "Not connected. Choose a network below.");
        lv_obj_set_style_text_font(s_status, APP_FONT_CAP, LV_PART_MAIN);
    }
    lv_obj_set_style_text_color(s_status, lv_color_hex(ui_text_mute()), LV_PART_MAIN);
}

/* Msgbox footer areas are a plain `lv_obj` with a label child; the indev target is
   usually the label, so CLICKED must be registered on the label too. */
static void mbox_bind_footer_clicked(lv_obj_t *btn, lv_event_cb_t cb, void *ud) {
    if (!btn || !cb) {
        return;
    }
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, ud);
    const uint32_t n = lv_obj_get_child_count(btn);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *ch = lv_obj_get_child(btn, i);
        if (ch) {
            lv_obj_add_event_cb(ch, cb, LV_EVENT_CLICKED, ud);
        }
    }
}

static void mbox_self_close_cb(lv_event_t *e) {
    lv_obj_t *mb = (lv_obj_t *)lv_event_get_user_data(e);
    if (mb && lv_obj_is_valid(mb)) {
        lv_msgbox_close(mb);
    }
}

/** Larger `lv_msgbox` for Wi‑Fi: width, title, body, footer, close control. */
static void wifi_msgbox_apply_large(lv_obj_t *mb) {
    if (!mb || !lv_obj_is_valid(mb)) {
        return;
    }
    const lv_coord_t w = (lv_coord_t)(APP_LCD_WIDTH * 90 / 100);
    if (w > 0) {
        lv_obj_set_width(mb, w);
    }
    lv_obj_set_style_max_width(mb, (lv_coord_t)(APP_LCD_WIDTH - APP_SAFE_INSET_H * 2), LV_PART_MAIN);
    lv_obj_set_style_pad_all(mb, 14, LV_PART_MAIN);

    lv_obj_t *title = lv_msgbox_get_title(mb);
    if (title) {
        lv_obj_set_style_text_font(title, APP_FONT_TITLE, LV_PART_MAIN);
    }
    lv_obj_t *hdr = lv_msgbox_get_header(mb);
    if (hdr) {
        lv_obj_set_style_min_height(hdr, 80, LV_PART_MAIN);
        lv_obj_set_style_pad_column(hdr, 8, LV_PART_MAIN);
        const uint32_t n = lv_obj_get_child_count(hdr);
        for (uint32_t i = 0; i < n; i++) {
            lv_obj_t *ch = lv_obj_get_child(hdr, i);
            if (ch == title) {
                continue;
            }
            /* Header close / extra buttons. */
            lv_obj_set_size(ch, 60, 60);
        }
    }

    lv_obj_t *content = lv_msgbox_get_content(mb);
    if (content) {
        lv_obj_set_style_pad_row(content, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_ver(content, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(content, 4, LV_PART_MAIN);
        const uint32_t n = lv_obj_get_child_count(content);
        for (uint32_t i = 0; i < n; i++) {
            lv_obj_t *ch = lv_obj_get_child(content, i);
            if (ch) {
                lv_obj_set_style_text_font(ch, APP_FONT_SUB, LV_PART_MAIN);
            }
        }
    }

    lv_obj_t *foot = lv_msgbox_get_footer(mb);
    if (foot) {
        lv_obj_set_style_min_height(foot, 88, LV_PART_MAIN);
        lv_obj_set_style_pad_top(foot, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(foot, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_column(foot, 14, LV_PART_MAIN);
        const uint32_t n = lv_obj_get_child_count(foot);
        for (uint32_t i = 0; i < n; i++) {
            lv_obj_t *btn = lv_obj_get_child(foot, i);
            if (!btn) {
                continue;
            }
            lv_obj_set_style_min_height(btn, 64, LV_PART_MAIN);
            lv_obj_set_style_pad_ver(btn, 12, LV_PART_MAIN);
            lv_obj_set_style_pad_hor(btn, 22, LV_PART_MAIN);
            if (lv_obj_get_child_count(btn) >= 1) {
                lv_obj_t *lab = lv_obj_get_child(btn, 0);
                if (lab) {
                    lv_obj_set_style_text_font(lab, APP_FONT_SUB, LV_PART_MAIN);
                }
            }
        }
    }
    (void)lv_obj_update_layout(mb);
}

/* ---------- result popup ---------- */
static void show_result_msg(void *) {
    const char *t = s_on_conn_info;
    s_on_conn_info = nullptr;
    if (!t) {
        t = (int)M5Comms.WiFi.status() == WL_CONNECTED ? "Connected." : "Connection failed.";
    }
    lv_obj_t *mb = lv_msgbox_create(nullptr);
    lv_msgbox_add_title(mb, "Wi-Fi");
    lv_msgbox_add_text(mb, t);
    lv_obj_t *bok = lv_msgbox_add_footer_button(mb, "OK");
    ui_mbox_bind_children_clicked(bok, mbox_self_close_cb, mb);
    ui_msgbox_add_close_and_backdrop(mb);
    wifi_msgbox_apply_large(mb);
    lv_obj_center(mb);
    shell_wifi_refresh();
    status_update();
    if (s_rebuild) {
        wifi_rebuild();
    }
}

/* ---------- connect (lv_timer, not FreeRTOS: WiFi.begin is unsafe on wconn) ---------- */
static void connect_timer_release() {
    if (s_connect_timer) {
        lv_timer_delete(s_connect_timer);
        s_connect_timer = NULL;
    }
    s_wifi_connecting.store(false, std::memory_order_release);
}

static void connect_drop_pending() {
    if (s_connect_timer) {
        void *u = lv_timer_get_user_data(s_connect_timer);
        connect_timer_release();
        if (u) {
            lv_free(u);
        }
    } else {
        s_wifi_connecting.store(false, std::memory_order_release);
    }
}

static void connect_done_ok(ConnSt *st) {
    connect_timer_release();
    if (!st) {
        return;
    }
    if (st->j.is_open) {
        arc_wifi_save(st->j.ssid, "");
    } else {
        arc_wifi_save(st->j.ssid, st->j.pass);
    }
    arc_net_invalidate();
    s_on_conn_info = kmsg_ok;
    lv_free(st);
    (void)lv_async_call(show_result_msg, nullptr);
}

static void connect_done_fail(ConnSt *st) {
    connect_timer_release();
    if (!st) {
        return;
    }
    s_on_conn_info = kmsg_fail;
    lv_free(st);
    (void)lv_async_call(show_result_msg, nullptr);
}

static void connect_timer_cb(lv_timer_t *t) {
    ConnSt *st = (ConnSt *)lv_timer_get_user_data(t);
    (void)t;
    if (!st) {
        connect_timer_release();
        return;
    }
#if ARC_DEBUG_WIFI
    wifi_dbgf("connect_timer: tick# %d  status=%d", (int)st->tick, (int)M5Comms.WiFi.status());
    Serial.flush();
#endif
    if ((int)M5Comms.WiFi.status() == WL_CONNECTED) {
        connect_done_ok(st);
        return;
    }
    st->tick++;
    if (st->tick >= 79) {
        connect_done_fail(st);
    }
}

static void start_connect(const char *ssid, const char *pass, bool is_open) {
#if ARC_DEBUG_WIFI
    wifi_dbgf("start_connect: enter ssid=%.32s open=%d", ssid ? ssid : "(null)", (int)is_open);
    Serial.flush();
#endif
    if (!M5Comms.isReady() || !ssid || !ssid[0]) {
#if ARC_DEBUG_WIFI
        wifi_dbg("start_connect: abort (not ready or no ssid)");
#endif
        return;
    }
    connect_drop_pending();
    ConnSt *st = (ConnSt *)lv_malloc(sizeof(ConnSt));
    if (!st) {
        return;
    }
    if (s_scan_running) {
        s_toast   = "Wait for the current scan to finish, then try again.";
        lv_free(st);
        (void)status_update();
        return;
    }
    s_wifi_connecting.store(true, std::memory_order_release);
    memset(st, 0, sizeof(*st));
    (void)snprintf(st->j.ssid, sizeof st->j.ssid, "%s", ssid);
    if (pass && pass[0] && !is_open) {
        (void)snprintf(st->j.pass, sizeof st->j.pass, "%s", pass);
    }
    st->j.is_open = is_open;
    s_toast = "Connecting…";
    (void)status_update();
#if ARC_DEBUG_WIFI
    wifi_dbg("start_connect: beginConnect (LVGL/loop context)");
    Serial.flush();
#endif
    (void)M5Comms.WiFi.beginConnect(st->j.ssid, st->j.is_open ? nullptr : (st->j.pass[0] ? st->j.pass : nullptr));
#if ARC_DEBUG_WIFI
    wifi_dbgf("start_connect: after beginConnect status=%d", (int)M5Comms.WiFi.status());
    Serial.flush();
#endif
    if ((int)M5Comms.WiFi.status() == WL_CONNECTED) {
        connect_done_ok(st);
        return;
    }
    s_connect_timer = lv_timer_create(connect_timer_cb, 250, st);
    if (!s_connect_timer) {
        connect_done_fail(st);
        return;
    }
    lv_timer_set_repeat_count(s_connect_timer, 79);
}

/* ---------- password modal ---------- */
static void pw_free(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }
    PwPack *p = (PwPack *)lv_event_get_user_data(e);
    if (p) {
        lv_free(p);
    }
}

static void pw_cancel(lv_event_t *e) {
    PwPack *p = (PwPack *)lv_event_get_user_data(e);
    if (p && p->root && lv_obj_is_valid(p->root)) {
        lv_obj_delete(p->root);
    }
}

static void pw_tap_outside_card(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    PwPack *p = (PwPack *)lv_event_get_user_data(e);
    if (!p || !p->dismiss_area) {
        return;
    }
    if (lv_event_get_target(e) != p->dismiss_area) {
        return;
    }
    pw_cancel(e);
}

static void pw_ok(lv_event_t *e) {
    PwPack *p = (PwPack *)lv_event_get_user_data(e);
    if (!p || !p->ta) {
        return;
    }
    /* `lv_textarea_get_text` points at the widget buffer; we must copy out before
     * `lv_obj_delete(p->root)` destroys the textarea (otherwise `start_connect` reads garbage). */
    char pass_copy[65];
    pass_copy[0] = '\0';
    const char *pw = lv_textarea_get_text(p->ta);
    if (pw) {
        (void)snprintf(pass_copy, sizeof pass_copy, "%s", pw);
    }
    char ss[33] = {0};
    (void)snprintf(ss, sizeof ss, "%s", p->ssid);
    if (p->root && lv_obj_is_valid(p->root)) {
        lv_obj_delete(p->root);
    }
    start_connect(ss, pass_copy, false);
}

static void show_password_dialog(const char *ssid) {
    PwPack *p = (PwPack *)lv_malloc(sizeof(PwPack));
    if (!p) {
        return;
    }
    memset(p, 0, sizeof(*p));
    (void)snprintf(p->ssid, sizeof p->ssid, "%s", ssid);

    lv_obj_t *modal = lv_obj_create(lv_layer_top());
    p->root         = modal;
    lv_obj_set_size(modal, lv_pct(100), lv_pct(100));
    lv_obj_set_layout(modal, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(modal, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(modal, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_bg_color(modal, lv_color_hex(0x0), LV_PART_MAIN);
    lv_obj_add_event_cb(modal, pw_free, LV_EVENT_DELETE, p);

    lv_obj_t *upper = lv_obj_create(modal);
    p->dismiss_area = upper;
    lv_obj_set_width(upper, lv_pct(100));
    lv_obj_set_flex_grow(upper, 1);
    /* Top-align the form. `lv_obj_center(card)` was vertically centering in this tall
     * area (all space above the keyboard), leaving a huge empty band above the card. */
    lv_obj_set_layout(upper, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(upper, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(upper, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_opa(upper, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(upper, 0, LV_PART_MAIN);
    /* Tight to under the shell / status chrome; was APP_SAFE_INSET_V and left a large gap. */
    lv_obj_set_style_pad_top(upper, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(upper, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(upper, 0, LV_PART_MAIN);
    lv_obj_add_flag(upper, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(upper, pw_tap_outside_card, LV_EVENT_CLICKED, p);

    lv_obj_t *card = lv_obj_create(upper);
    lv_obj_set_width(card, lv_pct(94));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    app_style_muted_card(card);
    lv_obj_set_style_pad_all(card, 16, LV_PART_MAIN);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 12, LV_PART_MAIN);

    lv_obj_t *head = lv_obj_create(card);
    lv_obj_set_width(head, lv_pct(100));
    lv_obj_set_style_bg_opa(head, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(head, 0, LV_PART_MAIN);
    lv_obj_set_layout(head, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(head, 0, LV_PART_MAIN);
    lv_obj_t *bx = lv_button_create(head);
    lv_obj_set_size(bx, 56, 56);
    shell_style_topbar_btn(bx);
    lv_obj_t *lx = lv_label_create(bx);
    lv_label_set_text(lx, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(lx, APP_FONT_TITLE, LV_PART_MAIN);
    lv_obj_center(lx);
    ui_mbox_bind_children_clicked(bx, pw_cancel, p);

    char L[100];
    (void)snprintf(L, sizeof L, "Password for\n%s", p->ssid);
    lv_obj_t *t0 = lv_label_create(card);
    lv_label_set_text(t0, L);
    lv_obj_set_style_text_font(t0, APP_FONT_HEADING, LV_PART_MAIN);
    lv_obj_set_style_text_color(t0, lv_color_hex(ui_text_primary()), LV_PART_MAIN);

    lv_obj_t *ta = lv_textarea_create(card);
    p->ta = ta;
    lv_obj_set_width(ta, lv_pct(100));
    lv_textarea_set_max_length(ta, 64);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, true);
    lv_obj_set_style_text_font(ta, APP_FONT_SUB, LV_PART_MAIN);

    lv_obj_t *r2 = lv_obj_create(card);
    lv_obj_set_width(r2, lv_pct(100));
    lv_obj_set_style_bg_opa(r2, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(r2, 0, LV_PART_MAIN);
    lv_obj_set_layout(r2, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(r2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r2, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r2, 16, LV_PART_MAIN);

    lv_obj_t *bc = lv_button_create(r2);
    lv_obj_set_style_min_height(bc, 64, LV_PART_MAIN);
    lv_obj_set_style_min_width(bc, 140, LV_PART_MAIN);
    lv_obj_t *lc = lv_label_create(bc);
    lv_label_set_text(lc, "Cancel");
    lv_obj_set_style_text_font(lc, APP_FONT_SUB, LV_PART_MAIN);
    lv_obj_center(lc);
    lv_obj_set_user_data(bc, p);
    ui_mbox_bind_children_clicked(bc, pw_cancel, p);

    lv_obj_t *bgo = lv_button_create(r2);
    lv_obj_set_style_min_height(bgo, 64, LV_PART_MAIN);
    lv_obj_set_style_min_width(bgo, 160, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bgo, lv_color_hex(APP_C_ACCENT), LV_PART_MAIN);
    lv_obj_t *lgo = lv_label_create(bgo);
    lv_label_set_text(lgo, "Connect");
    lv_obj_set_style_text_color(lgo, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lgo, APP_FONT_SUB, LV_PART_MAIN);
    lv_obj_center(lgo);
    lv_obj_set_user_data(bgo, p);
    ui_mbox_bind_children_clicked(bgo, pw_ok, p);

    /* Phone-style: keyboard is a sibling, full width at the bottom of the screen. */
    lv_coord_t kh = (lv_coord_t)((int)APP_LCD_HEIGHT * 34 / 100);
    if (kh < 200) {
        kh = 200;
    }
    if (kh > 520) {
        kh = 520;
    }
    lv_obj_t *kb = lv_keyboard_create(modal);
    lv_obj_set_width(kb, lv_pct(100));
    lv_obj_set_height(kb, kh);
    lv_obj_set_style_radius(kb, 0, LV_PART_MAIN);
    /* Default lv_obj is SCROLLABLE + scroll-chain; any pointer jitter starts
       lv_indev_scroll_handler, which drops PRESSED on the key matrix. Block that. */
    lv_obj_remove_flag(kb, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(kb, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_remove_flag(modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(upper, LV_OBJ_FLAG_SCROLLABLE);
    /* lv_keyboard's ctor aligns to bottom; we lay out in a column flex. */
    lv_obj_set_align(kb, LV_ALIGN_DEFAULT);
    lv_obj_set_pos(kb, 0, 0);
    (void)lv_obj_update_layout(modal);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(kb, ta);
}

/* ---------- name popup (long SSID) ---------- */
static void name_popup(const char *full) {
    if (!full) {
        return;
    }
    lv_obj_t *mb = lv_msgbox_create(nullptr);
    lv_msgbox_add_title(mb, "Full network name");
    lv_msgbox_add_text(mb, full);
    lv_obj_t *bok = lv_msgbox_add_footer_button(mb, "OK");
    ui_mbox_bind_children_clicked(bok, mbox_self_close_cb, mb);
    ui_msgbox_add_close_and_backdrop(mb);
    wifi_msgbox_apply_large(mb);
    lv_obj_center(mb);
}

/* ---------- action menu (connect / forget) ---------- */
struct MenuCtx {
    lv_obj_t *mb;
    char      ssid[33];
    int       enc;
    bool      saved;
};

static void menu_ctx_on_delete(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }
    MenuCtx *c = (MenuCtx *)lv_event_get_user_data(e);
    if (c) {
        lv_free(c);
    }
}

static void close_menu(void *c) {
    MenuCtx *m = (MenuCtx *)c;
    if (m && m->mb && lv_obj_is_valid(m->mb)) {
        lv_msgbox_close(m->mb);
    }
}

static void menu_forget(lv_event_t *e) {
    MenuCtx *c = (MenuCtx *)lv_event_get_user_data(e);
    if (!c) {
        return;
    }
    (void)arc_wifi_forget();
    if (M5Comms.isReady()) {
        (void)M5Comms.WiFi.disconnect(false, true);
    }
    arc_net_invalidate();
    close_menu(c);
    shell_wifi_refresh();
    status_update();
    wifi_rebuild();
}

static void menu_connect_row(lv_event_t *e) {
    MenuCtx *c = (MenuCtx *)lv_event_get_user_data(e);
    if (!c) {
        return;
    }
    if (c->enc == (int)WIFI_AUTH_OPEN) {
        start_connect(c->ssid, nullptr, true);
        close_menu(c);
    } else if (c->saved) {
        char p[65], s[33];
        if (arc_wifi_get_saved(s, sizeof s, p, sizeof p) && strncmp(s, c->ssid, 33) == 0) {
            start_connect(c->ssid, p, false);
        }
        close_menu(c);
    } else {
        char ss[33] = {0};
        (void)snprintf(ss, sizeof ss, "%s", c->ssid);
        close_menu(c);
        show_password_dialog(ss);
    }
}

/** Drop current AP association; does not clear saved creds (unlike Forget). */
static void menu_disconnect_sta(lv_event_t *e) {
    MenuCtx *c = (MenuCtx *)lv_event_get_user_data(e);
    if (!c) {
        return;
    }
    if (M5Comms.isReady()) {
        (void)M5Comms.WiFi.disconnect(false, false);
    }
    arc_net_invalidate();
    close_menu(c);
    shell_wifi_refresh();
    status_update();
    wifi_rebuild();
}

static void show_network_menu(const char *ssid, int enc) {
    if (!ssid) {
        return;
    }
    MenuCtx *c = (MenuCtx *)lv_malloc(sizeof(MenuCtx));
    if (!c) {
        return;
    }
    memset(c, 0, sizeof(*c));
    c->enc   = enc;
    c->saved = arc_wifi_is_saved(ssid);
    (void)snprintf(c->ssid, sizeof c->ssid, "%s", ssid);

    lv_obj_t *mb = lv_msgbox_create(nullptr);
    c->mb        = mb;
    lv_obj_add_event_cb(mb, menu_ctx_on_delete, LV_EVENT_DELETE, c);
    lv_msgbox_add_title(mb, c->ssid);
    lv_msgbox_add_text(mb, c->saved ? "This network is saved on device." : "Connect to this network?");

    lv_obj_t *b1 = lv_msgbox_add_footer_button(mb, "Connect");
    ui_mbox_bind_children_clicked(b1, menu_connect_row, c);
    if (c->saved) {
        lv_obj_t *b2 = lv_msgbox_add_footer_button(mb, "Forget");
        ui_mbox_bind_children_clicked(b2, menu_forget, c);
    }
    lv_obj_t *b3 = lv_msgbox_add_footer_button(mb, "Disconnect");
    ui_mbox_bind_children_clicked(b3, menu_disconnect_sta, c);
    ui_msgbox_add_close_and_backdrop(mb);
    wifi_msgbox_apply_large(mb);
    lv_obj_center(mb);
}

/* ---------- row & list ---------- */
static void row_free_ud(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }
    RowU *p = (RowU *)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
    if (p) {
        lv_free(p);
    }
}

static void row_tap_name(lv_event_t *e) {
    LvglHal::click_feedback();
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    RowU *p = (RowU *)lv_event_get_user_data(e);
    if (p) {
        show_network_menu(p->full_ssid, p->enc);
    }
}

static void row_tap_ellipsis(lv_event_t *e) {
    LvglHal::click_feedback();
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    RowU *p = (RowU *)lv_event_get_user_data(e);
    if (p) {
        name_popup(p->full_ssid);
    }
}

/* Default theme `card` on plain `lv_obj` (pad_row/column + pad_all). */
static void wifi_theme_clear_obj_pad(lv_obj_t *o) {
    if (!o) {
        return;
    }
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(o, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(o, 0, LV_PART_MAIN);
}

/** Clicks on button labels: bind same handler + RowU* (see ui_mbox note in views_internal). */
static void wifi_bind_clicked_to_btn_children(lv_obj_t *b, void (*cb)(lv_event_t *), void *ud) {
    if (!b || !cb) {
        return;
    }
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    const uint32_t n = lv_obj_get_child_count(b);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *ch = lv_obj_get_child(b, i);
        if (ch) {
            lv_obj_add_event_cb(ch, cb, LV_EVENT_CLICKED, ud);
        }
    }
}

/* Full-width list rows; same vertical sizing and `settings_sidebar_btn_style` as Settings sidebar buttons. */
static void wifi_add_settings_sidebar_item(lv_obj_t *list, const WifiApRow *a, bool is_current) {
    RowU *p = (RowU *)lv_malloc(sizeof(RowU));
    if (!p) {
        return;
    }
    memset(p, 0, sizeof(*p));
    (void)snprintf(p->full_ssid, sizeof p->full_ssid, "%s", a->ssid);
    p->enc = a->enc;

    lv_obj_t *b = lv_button_create(list);
    lv_obj_set_width(b, lv_pct(100));
    lv_obj_set_height(b, LV_SIZE_CONTENT);
    settings_sidebar_btn_style(b, is_current);
    lv_obj_set_layout(b, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_min_height(b, 108, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(b, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(b, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_row(b, 6, LV_PART_MAIN);
    lv_obj_set_user_data(b, p);
    lv_obj_add_event_cb(b, row_free_ud, LV_EVENT_DELETE, nullptr);

    lv_obj_t *ic = lv_label_create(b);
    wifi_theme_clear_obj_pad(ic);
    lv_label_set_text(ic, is_current ? LV_SYMBOL_OK : LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(ic, APP_FONT_HERO, LV_PART_MAIN);
    lv_obj_set_style_text_color(ic, lv_color_hex(APP_C_ICON_ACCENT_BLUE), LV_PART_MAIN);
    lv_obj_set_style_text_align(ic, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    lv_obj_t *cap = lv_label_create(b);
    wifi_theme_clear_obj_pad(cap);
    lv_label_set_text(cap, a->ssid);
    /* Larger than body for network names; meta line stays APP_FONT_CAP. */
    lv_obj_set_style_text_font(cap, APP_FONT_SUB, LV_PART_MAIN);
    /* Match `mk_side` in view_settings: body caption is muted. */
    lv_obj_set_style_text_color(cap, lv_color_hex(ui_text_mute()), LV_PART_MAIN);
    lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(cap, lv_pct(100));
    lv_label_set_long_mode(cap, LV_LABEL_LONG_MODE_DOTS);

    {
        char   meta[64];
        const char *encs = (a->enc == (int)WIFI_AUTH_OPEN) ? "Open" : "Secured";
        /* ASCII only — U+00B7 middle dot is not in the baked Montserrat subset (shows as a square). */
        (void)snprintf(meta, sizeof meta, "%d dBm  -  %s", (int)a->rssi, encs);
        lv_obj_t *m = lv_label_create(b);
        wifi_theme_clear_obj_pad(m);
        lv_label_set_text(m, meta);
        lv_obj_set_style_text_font(m, APP_FONT_CAP, LV_PART_MAIN);
        lv_obj_set_style_text_color(m, lv_color_hex(ui_text_mute()), LV_PART_MAIN);
        lv_obj_set_style_text_align(m, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_width(m, lv_pct(100));
        lv_label_set_long_mode(m, LV_LABEL_LONG_MODE_DOTS);
    }

    wifi_bind_clicked_to_btn_children(b, row_tap_name, p);
}

static void wifi_list_muted_caption(lv_obj_t *list, const char *msg) {
    lv_obj_t *l = lv_label_create(list);
    wifi_theme_clear_obj_pad(l);
    lv_label_set_text(l, msg);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_text_font(l, APP_FONT_CAP, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(ui_text_mute()), LV_PART_MAIN);
    lv_obj_set_style_pad_left(l, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_top(l, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(l, 6, LV_PART_MAIN);
}

static void wifi_rebuild() {
#if ARC_DEBUG_WIFI
    wifi_dbg("wifi_rebuild: enter");
#endif
    if (!s_list || !lv_obj_is_valid(s_list)) {
#if ARC_DEBUG_WIFI
        wifi_dbg("wifi_rebuild: early exit (no list)");
#endif
        return;
    }
    lv_obj_clean(s_list);
    xSemaphoreTake(s_aps_mutex, portMAX_DELAY);
    std::vector<WifiApRow> aps   = s_aps;
    char                   err[96] = {0};
    if (s_scan_err[0]) {
        (void)snprintf(err, sizeof err, "%s", s_scan_err);
    }
    s_scan_err[0] = 0;
    xSemaphoreGive(s_aps_mutex);

    if (err[0] && aps.empty()) {
        lv_obj_t *b = lv_label_create(s_list);
        (void)lv_label_set_text_fmt(b, "Scan: %s", err);
        lv_obj_set_style_text_font(b, APP_FONT_CAP, LV_PART_MAIN);
        lv_obj_set_style_text_color(b, lv_color_hex(ui_text_mute()), LV_PART_MAIN);
        return;
    }

    /* Split: current (connected) · other APs (macOS-style). */
    std::vector<WifiApRow>       cur_aps;
    std::vector<WifiApRow>       other_aps;
    const bool                  comms_ok = M5Comms.isReady();
    const bool                  is_conn  = comms_ok && (int)M5Comms.WiFi.status() == WL_CONNECTED;
    if (is_conn) {
        String csn = M5Comms.WiFi.connectedSSID();
        bool   had = false;
        if (csn.length() > 0) {
            for (const auto &a : aps) {
                if (csn == a.ssid) {
                    cur_aps.push_back(a);
                    had = true;
                    break;
                }
            }
            if (!had) {
                WifiApRow s {};
                csn.toCharArray(s.ssid, (unsigned)sizeof s.ssid);
                s.rssi     = (int32_t)M5Comms.WiFi.connectedRSSI();
                s.channel  = 0;
                s.enc      = (int)WIFI_AUTH_WPA2_PSK;
                cur_aps.push_back(s);
            }
        }
        for (const auto &a : aps) {
            if (csn.length() && csn == a.ssid) {
                continue;
            }
            other_aps.push_back(a);
        }
    } else {
        other_aps = aps;
    }

    /* One full-width button per network, settings_sidebar_btn_style (heights match Settings). */
    struct RowItem {
        WifiApRow ap;
        bool      current;
    };
    std::vector<RowItem> rows;
    rows.reserve((size_t)cur_aps.size() + other_aps.size());
    if (is_conn && !cur_aps.empty()) {
        RowItem ri {};
        ri.ap      = cur_aps[0];
        ri.current = true;
        rows.push_back(ri);
    }
    for (const auto &a : other_aps) {
        RowItem ri {};
        ri.ap      = a;
        ri.current = false;
        rows.push_back(ri);
    }

    if (rows.empty()) {
        if (s_tap_to_scan_hint) {
            wifi_list_muted_caption(s_list, "Tap \"Scan for networks\" to search.");
        } else {
            wifi_list_muted_caption(s_list, "No networks in range.");
        }
    } else {
        for (const auto &ri : rows) {
            wifi_add_settings_sidebar_item(s_list, &ri.ap, ri.current);
        }
    }
    if (s_list && lv_obj_is_valid(s_list)) {
        lv_obj_set_flex_grow(s_list, 1);
        lv_obj_t *pl = lv_obj_get_parent(s_list);
        (void)lv_obj_update_layout(s_list);
        if (pl && lv_obj_is_valid(pl)) {
            (void)lv_obj_update_layout(pl);
        }
    }
#if ARC_DEBUG_WIFI
    wifi_dbg("wifi_rebuild: done");
#endif
}

/* ---------- scan ---------- */
static void wifi_scan_task(void *arg) {
    (void)arg;
#if ARC_DEBUG_WIFI
    wifi_dbg("wifi_scan_task: 1 start");
    Serial.flush();
#endif
    s_scan_err[0] = 0;
    std::vector<WifiApRow> local;
    if (s_wifi_connecting.load(std::memory_order_acquire)) {
        (void)snprintf(s_scan_err, sizeof s_scan_err, "Connecting. Try again in a few seconds.");
    } else if (!M5Comms.isReady()) {
#if ARC_DEBUG_WIFI
        wifi_dbg("wifi_scan_task: 2 isReady=0");
#endif
        (void)snprintf(s_scan_err, sizeof s_scan_err, "C6 not ready.");
    } else {
#if ARC_DEBUG_WIFI
        wifi_dbg("wifi_scan_task: 3 before scanNetworks");
        Serial.flush();
#endif
        const int n = M5Comms.WiFi.scanNetworks();
#if ARC_DEBUG_WIFI
        wifi_dbgf("wifi_scan_task: 4 after scanNetworks n=%d", n);
        Serial.flush();
#endif
        if (n < 0) {
            (void)snprintf(s_scan_err, sizeof s_scan_err, "Scan failed.");
        } else {
            local.reserve((size_t)n);
            for (int i = 0; i < n; i++) {
                const String s = M5Comms.WiFi.SSID(i);
                if (s.length() == 0) {
                    continue;
                }
                WifiApRow w {};
                s.toCharArray(w.ssid, (unsigned)sizeof w.ssid);
                w.rssi     = (int32_t)M5Comms.WiFi.RSSI(i);
                w.channel  = (uint8_t)M5Comms.WiFi.channel(i);
                w.enc      = M5Comms.WiFi.encryptionType(i);
                local.push_back(w);
            }
#if ARC_DEBUG_WIFI
            wifi_dbgf("wifi_scan_task: 5 collected %u APs (before scanDelete)", (unsigned)local.size());
            Serial.flush();
#endif
            M5Comms.WiFi.scanDelete();
#if ARC_DEBUG_WIFI
            wifi_dbg("wifi_scan_task: 6 after scanDelete");
            Serial.flush();
#endif
        }
    }
    std::sort(
        local.begin(), local.end(), [](const WifiApRow &a, const WifiApRow &b) { return a.rssi > b.rssi; });
    xSemaphoreTake(s_aps_mutex, portMAX_DELAY);
    s_aps          = std::move(local);
    s_scan_done    = true;
    s_scan_running = false;
    xSemaphoreGive(s_aps_mutex);
#if ARC_DEBUG_WIFI
    wifi_dbg("wifi_scan_task: 7 done (s_scan_done=1)");
    Serial.flush();
#endif
    vTaskDelete(NULL);
}

static void wifi_start_scan() {
    if (s_scan_running) {
        return;
    }
    if (s_wifi_connecting.load(std::memory_order_acquire)) {
        s_toast = "Still connecting. Try scan again in a few seconds.";
        (void)status_update();
        return;
    }
#if ARC_DEBUG_WIFI
    wifi_dbg("wifi_start_scan: creating wscan task");
    Serial.flush();
#endif
    s_tap_to_scan_hint = false;
    s_scan_running = true;
    s_scan_done    = false;
    if (xTaskCreate(wifi_scan_task, "wscan", 12000, NULL, 1, NULL) != pdPASS) {
#if ARC_DEBUG_WIFI
        wifi_dbg("wifi_start_scan: xTaskCreate FAILED");
#endif
        s_scan_running = false;
        (void)snprintf(s_scan_err, sizeof s_scan_err, "Task failed");
        s_scan_done = true;
    }
    s_toast = "Scanning…";
    status_update();
}

/* ---------- poll & view ---------- */
static void wifi_poll_cb(lv_timer_t *t) {
    (void)t;
    if (s_scan_done) {
#if ARC_DEBUG_WIFI
        wifi_dbg("wifi_poll_cb: scan finished → status_update + rebuild");
        Serial.flush();
#endif
        s_scan_done = false;
        status_update();
        wifi_rebuild();
    } else {
        status_update();
    }
}

static void wifi_scan_btn_cb(lv_event_t *e) {
    (void)e;
    LvglHal::click_feedback();
    wifi_start_scan();
}

void view_wifi() {
#if ARC_DEBUG_WIFI
    wifi_dbg("view_wifi: 1 enter");
    Serial.flush();
#endif
    connect_drop_pending();
    if (!s_aps_mutex) {
        s_aps_mutex = xSemaphoreCreateMutex();
    }
    if (s_poll) {
        lv_timer_delete(s_poll);
        s_poll = NULL;
    }
    s_rebuild = +[]() { wifi_rebuild(); };
    shell_mount("Wi-Fi", nullptr, nullptr, false);
#if ARC_DEBUG_WIFI
    wifi_dbg("view_wifi: 2 after shell_mount");
    Serial.flush();
#endif
    s_list   = NULL;
    s_status = NULL;
    s_scan_done      = false;
    s_tap_to_scan_hint = true;

    lv_obj_t *c = content_ptr();
    lv_obj_set_width(c, lv_pct(100));
    lv_obj_set_layout(c, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    /* Without an explicit full-width child, a column flex can size the only grow
       item to content width, leaving a narrow column on a wide display. */
    lv_obj_set_style_pad_column(c, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(c, 0, LV_PART_MAIN);
    /* Tight top, keep horizontal + bottom safe insets. */
    lv_obj_set_style_pad_left(c, (lv_coord_t)APP_SAFE_INSET_H, LV_PART_MAIN);
    lv_obj_set_style_pad_right(c, (lv_coord_t)APP_SAFE_INSET_H, LV_PART_MAIN);
    lv_obj_set_style_pad_top(c, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(c, (lv_coord_t)APP_SAFE_INSET_V, LV_PART_MAIN);

    /* Single full-width main panel (no narrow side rail). Row heights match Settings sidebar buttons. */
    lv_obj_t *panel = lv_obj_create(c);
    wifi_theme_clear_obj_pad(panel);
    lv_obj_set_width(panel, lv_pct(100));
    lv_obj_set_flex_grow(panel, 1);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    {
        lv_obj_t *bscan = lv_button_create(panel);
        lv_obj_set_width(bscan, lv_pct(100));
        lv_obj_set_height(bscan, LV_SIZE_CONTENT);
        settings_sidebar_btn_style(bscan, false);
        lv_obj_set_layout(bscan, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(bscan, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(bscan, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_min_height(bscan, 108, LV_PART_MAIN);
        lv_obj_set_style_pad_ver(bscan, 18, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(bscan, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_row(bscan, 6, LV_PART_MAIN);
        lv_obj_t *ic = lv_label_create(bscan);
        wifi_theme_clear_obj_pad(ic);
        lv_label_set_text(ic, LV_SYMBOL_REFRESH);
        lv_obj_set_style_text_font(ic, APP_FONT_HERO, LV_PART_MAIN);
        lv_obj_set_style_text_color(ic, lv_color_hex(APP_C_ICON_ACCENT_BLUE), LV_PART_MAIN);
        lv_obj_set_style_text_align(ic, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_t *cap = lv_label_create(bscan);
        wifi_theme_clear_obj_pad(cap);
        lv_label_set_text(cap, "Scan for networks");
        lv_obj_set_style_text_font(cap, APP_FONT_BODY, LV_PART_MAIN);
        lv_obj_set_style_text_color(cap, lv_color_hex(ui_text_mute()), LV_PART_MAIN);
        lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_width(cap, lv_pct(100));
        lv_label_set_long_mode(cap, LV_LABEL_LONG_MODE_DOTS);
        wifi_bind_clicked_to_btn_children(bscan, wifi_scan_btn_cb, nullptr);
    }

    s_status = lv_label_create(panel);
    wifi_theme_clear_obj_pad(s_status);
    lv_obj_set_width(s_status, lv_pct(100));
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(s_status, 6, LV_PART_MAIN);
    status_update();
#if ARC_DEBUG_WIFI
    wifi_dbg("view_wifi: 3 after first status_update (s_list not created yet)");
    Serial.flush();
#endif
    s_list = lv_obj_create(panel);
    wifi_theme_clear_obj_pad(s_list);
    lv_obj_set_width(s_list, lv_pct(100));
    lv_obj_set_flex_grow(s_status, 0);
    lv_obj_set_flex_grow(s_list, 1);
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    apply_scroll_tabled(s_list);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_layout(s_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    /* Match `sidebar` in view_settings. */
    lv_obj_set_style_pad_top(s_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(s_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_list, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_list, 0, LV_PART_MAIN);

    s_poll = lv_timer_create(wifi_poll_cb, 400, NULL);
    lv_timer_set_repeat_count(s_poll, -1);
#if ARC_DEBUG_WIFI
    wifi_dbg("view_wifi: 4 before wifi_rebuild + poll running");
    Serial.flush();
#endif
    wifi_rebuild();
#if ARC_DEBUG_WIFI
    wifi_dbg("view_wifi: 5 leave (ok)");
    Serial.flush();
#endif
}
