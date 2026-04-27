#include "Views.hpp"
#include "AppTypes.hpp"
#include "LvglHal.hpp"
#include "ogsm/ogsm_service.hpp"
#include "ogsm/ogsm_config.hpp"
#include "ogsm/RadioManager.hpp"
#include "views_config.hpp"
#include "views_internal.hpp"
#include "app_theme.hpp"
#include "arcane_lvgl.h"
#include <Arduino.h>
#include <M5Unified.h>
#include <stdio.h>
#include <string.h>

using ogsm::ChatLine;
using ogsm::MsgStatus;
using ogsm::RadioPath;

static const char *st_icon(MsgStatus s) {
    switch (s) {
    case MsgStatus::Read: return "[R]";
    case MsgStatus::Delivered: return "[✓✓]";
    case MsgStatus::NextHop: return "[✓]";
    case MsgStatus::Sending:
    default: return "[.]";
    }
}

static bool parse_id6(const char *s, uint8_t out[6]) {
    if (!s || !out) return false;
    /* Accept "" as broadcast */
    if (!s[0]) {
        memset(out, 0xFF, 6u);
        return true;
    }
    unsigned v[6];
    if (sscanf(s, "%02x:%02x:%02x:%02x:%02x:%02x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) out[i] = (uint8_t)v[i];
    return true;
}

static void rebuild_log(lv_obj_t *lab) {
    std::vector<ChatLine> L;
    ogsm::get_lines(L);
    char   buf[4200] = {0};
    size_t o = 0u;
    for (const ChatLine &ln : L) {
        const char *who = ln.mine ? "Me" : "Peer";
        int rem = (int)sizeof buf - (int)o;
        if (rem < 16) break;
        int nw = snprintf(buf + o, (size_t)rem, "%s %s %s\n", who, st_icon(ln.status), ln.text);
        if (nw < 0) break;
        o += (size_t)nw;
    }
    lv_label_set_text(lab, buf);
}

struct OgsmUi {
    lv_obj_t   *log;
    lv_obj_t   *ta;
    lv_obj_t   *target;
    lv_obj_t   *dd;
    lv_obj_t   *caps;
    lv_timer_t *tmr;
};

static void caps_refresh(lv_obj_t *lab) {
    if (!lab || !lv_obj_is_valid(lab)) {
        return;
    }
    auto &rm = ogsm::RadioManager::instance();
    const bool esp = rm.espnow_ok();
    const bool lor = rm.lora_ok();
    char b[96];
    snprintf(b, sizeof(b), "ESP-NOW available: %s   ·   LoRa available: %s", esp ? "Yes" : "No", lor ? "Yes" : "No");
    lv_label_set_text(lab, b);
}

static void send_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    LvglHal::click_feedback();
    OgsmUi *ui = (OgsmUi *)lv_event_get_user_data(e);
    if (!ui || !ui->ta || !ui->log || !ui->dd || !ui->target) return;
    const char *t = lv_textarea_get_text(ui->ta);
    if (!t || !t[0]) return;
    uint8_t id[6];
    if (!parse_id6(lv_textarea_get_text(ui->target), id)) {
        return;
    }
    const uint32_t sel = (uint32_t)lv_dropdown_get_selected(ui->dd);
    RadioPath p = (sel == 0) ? RadioPath::EspNow : (sel == 1) ? RadioPath::LoRa : RadioPath::Both;
    (void)ogsm::send_text(t, id, p);
    lv_textarea_set_text(ui->ta, "");
    rebuild_log(ui->log);
    caps_refresh(ui->caps);
    shell_ogsm_refresh();
}

static void tmr_cb(lv_timer_t *t) {
    OgsmUi *ui = (OgsmUi *)lv_timer_get_user_data(t);
    if (!ui) {
        lv_timer_delete(t);
        return;
    }
    /* If we've navigated away, LVGL will have deleted the widgets. Stop the timer to avoid crashes. */
    if (!ui->log || !lv_obj_is_valid(ui->log) || !ui->caps || !lv_obj_is_valid(ui->caps)) {
        ui->tmr = nullptr;
        lv_timer_delete(t);
        return;
    }
    rebuild_log(ui->log);
    caps_refresh(ui->caps);
    shell_ogsm_refresh();
}

void view_ogsm() {
    shell_mount("OGSM", nullptr, nullptr, false);
    lv_obj_t *c = content_ptr();
    color_bg(c, ui_bg_content());
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c, 10, LV_PART_MAIN);

    static OgsmUi ui_s;
    OgsmUi       *ui = &ui_s;
    if (ui->tmr) {
        lv_timer_delete(ui->tmr);
        ui->tmr = nullptr;
    }
    memset(ui, 0, sizeof *ui);

    lv_obj_t *hdr = lv_obj_create(c);
    lv_obj_set_width(hdr, lv_pct(100));
    app_style_muted_card(hdr);
    /* Stack title + capability line; default layout overlaps. */
    lv_obj_set_layout(hdr, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(hdr, 6, LV_PART_MAIN);
    lv_obj_t *h = lv_label_create(hdr);
    lv_label_set_text(h, "Off Grid Secure Messaging");
    lv_obj_set_style_text_font(h, APP_FONT_HEADING, LV_PART_MAIN);
    lv_obj_set_style_text_color(h, lv_color_hex(ui_text_primary()), LV_PART_MAIN);
    ui->caps = lv_label_create(hdr);
    lv_obj_set_width(ui->caps, lv_pct(100));
    lv_obj_set_style_text_font(ui->caps, APP_FONT_SUB, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui->caps, lv_color_hex(ui_text_mute()), LV_PART_MAIN);
    lv_label_set_long_mode(ui->caps, LV_LABEL_LONG_MODE_WRAP);
    caps_refresh(ui->caps);

    lv_obj_t *row = lv_obj_create(c);
    lv_obj_set_width(row, lv_pct(100));
    app_style_muted_card(row);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 10, LV_PART_MAIN);
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(row, 8, LV_PART_MAIN);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ui->dd = lv_dropdown_create(row);
    lv_dropdown_set_options(ui->dd, "ESP-NOW\nLoRa\nBoth");
    lv_dropdown_set_selected(ui->dd, 2u);

    ui->target = lv_textarea_create(row);
    lv_textarea_set_one_line(ui->target, true);
    lv_textarea_set_placeholder_text(ui->target, "Target MAC (empty=broadcast)");
    lv_obj_set_flex_grow(ui->target, 1);

    ui->log = lv_label_create(c);
    lv_obj_set_width(ui->log, lv_pct(100));
    lv_obj_set_flex_grow(ui->log, 1);
    lv_obj_set_style_text_font(ui->log, APP_FONT_BODY, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui->log, lv_color_hex(ui_text_primary()), LV_PART_MAIN);
    lv_label_set_long_mode(ui->log, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_add_flag(ui->log, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(ui->log, 40, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui->log, lv_color_hex(0xE8E8ED), LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui->log, 10, LV_PART_MAIN);
    /* Ensure OGSM timer can't outlive this screen. */
    lv_obj_add_event_cb(
        ui->log,
        [](lv_event_t *e) {
            if (lv_event_get_code(e) != LV_EVENT_DELETE) {
                return;
            }
            OgsmUi *ui = (OgsmUi *)lv_event_get_user_data(e);
            if (ui && ui->tmr) {
                lv_timer_delete(ui->tmr);
                ui->tmr = nullptr;
            }
        },
        LV_EVENT_DELETE, ui);

    ui->ta = lv_textarea_create(c);
    lv_obj_set_width(ui->ta, lv_pct(100));
    lv_obj_set_height(ui->ta, 52);
    lv_textarea_set_one_line(ui->ta, true);
    lv_textarea_set_placeholder_text(ui->ta, "Message…");

    lv_obj_t *btn = lv_button_create(c);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_t *bl = lv_label_create(btn);
    lv_label_set_text(bl, "Send");
    lv_obj_center(bl);
    lv_obj_add_event_cb(btn, send_cb, LV_EVENT_CLICKED, ui);

    rebuild_log(ui->log);
    caps_refresh(ui->caps);
    ui->tmr = lv_timer_create(tmr_cb, 900, ui);
    lv_timer_set_repeat_count(ui->tmr, -1);
}
