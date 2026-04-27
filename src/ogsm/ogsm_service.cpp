#include "ogsm/ogsm_service.hpp"
#include "ogsm/RadioManager.hpp"
#include <Arduino.h>
#include <cstring>
#include <esp_random.h>

namespace ogsm {

static bool                 s_inited   = false;
static bool                 s_fg       = false; /* OGSM screen in foreground */
static uint32_t             s_unread   = 0;
static RadioPath            s_path     = RadioPath::Both;
static std::vector<ChatLine> s_lines;

static bool id_eq_(const uint8_t a[6], const uint8_t b[6]) { return memcmp(a, b, 6) == 0; }

static void mk_line_from_pkt_(ChatLine &l, const ArcanePacket &p, bool mine) {
    l.msgId  = p.msgId;
    l.mine   = mine;
    l.unread = !mine && !s_fg;
    l.status = mine ? MsgStatus::Sending : MsgStatus::NextHop;
    memcpy(l.peer, mine ? p.target : p.sender, 6u);
    const size_t n = (size_t)p.payloadLen;
    const size_t c = (n < sizeof l.text) ? n : (sizeof l.text) - 1u;
    memcpy(l.text, p.payload, c);
    l.text[c] = 0;
}

void service_init() {
    if (s_inited) {
        return;
    }
    s_inited = true;
    s_unread = 0;
    s_lines.clear();
    RadioConfig rc;
    rc.prefer   = s_path;
    rc.mesh_ttl = 3u;
    (void)RadioManager::instance().init(rc);
}

void service_poll() {
    if (!s_inited) {
        return;
    }
    RadioManager::instance().poll();
}

void on_app_shown(AppScreen s) {
    const bool now_fg = (s == AppScreen::Ogsm);
    if (now_fg && !s_fg) {
        /* entering OGSM: clear unread and emit ACK_READ for unread incoming */
        s_unread = 0;
        if (s_inited) {
            for (ChatLine &ln : s_lines) {
                if (ln.mine || !ln.unread) {
                    continue;
                }
                ln.unread = false;
                ArcanePacket a = {};
                a.type    = PacketType::ACK_READ;
                a.msgId   = ln.msgId;
                a.hopCount = 3u;
                memcpy(a.sender, RadioManager::instance().local_id(), 6u);
                memcpy(a.target, ln.peer, 6u);
                (void)RadioManager::instance().sendMessage(a, s_path);
            }
        }
    }
    s_fg = now_fg;
}

unsigned unread_count() { return (unsigned)s_unread; }

void get_lines(std::vector<ChatLine> &out) { out = s_lines; }

static ChatLine *find_mine_(uint32_t id) {
    for (ChatLine &l : s_lines) {
        if (l.mine && l.msgId == id) {
            return &l;
        }
    }
    return nullptr;
}

bool send_text(const char *utf8, const uint8_t target[6], RadioPath path) {
    if (!s_inited) {
        service_init();
    }
    s_path = path;
    RadioConfig rc;
    rc.prefer   = s_path;
    rc.mesh_ttl = 3u;
    RadioManager::instance().set_config(rc);

    ArcanePacket p = {};
    p.type     = PacketType::CHAT;
    p.msgId    = (uint32_t)esp_random();
    if (p.msgId == 0u) {
        p.msgId = 1u;
    }
    p.hopCount = rc.mesh_ttl;
    memcpy(p.sender, RadioManager::instance().local_id(), 6u);
    memcpy(p.target, target, 6u);
    p.set_payload_text(utf8);

    ChatLine ln = {};
    mk_line_from_pkt_(ln, p, true);
    ln.status = MsgStatus::Sending;
    s_lines.push_back(ln);

    const bool ok = RadioManager::instance().sendMessage(p, s_path);
    if (ok) {
        if (ChatLine *m = find_mine_(p.msgId)) {
            m->status = MsgStatus::NextHop;
        }
    }
    return ok;
}

void on_rx_packet(const ArcanePacket &p, int link, std::vector<ArcanePacket> &out_replies) {
    (void)link;
    if (p.type == PacketType::HEARTBEAT) {
        return;
    }
    if (p.type == PacketType::ACK_DELIVERED) {
        if (ChatLine *m = find_mine_(p.msgId)) {
            if (m->status < MsgStatus::Delivered) {
                m->status = MsgStatus::Delivered;
            }
        }
        return;
    }
    if (p.type == PacketType::ACK_READ) {
        if (ChatLine *m = find_mine_(p.msgId)) {
            m->status = MsgStatus::Read;
        }
        return;
    }
    if (p.type != PacketType::CHAT) {
        return;
    }

    /* Accept broadcast or targeted to us; RadioManager already filtered for target. */
    ChatLine ln = {};
    mk_line_from_pkt_(ln, p, false);
    s_lines.push_back(ln);
    if (ln.unread) {
        s_unread++;
    }

    /* Auto ACK_DELIVERED for unicast to us. */
    if (!id_is_broadcast(p.target) && id_eq_(p.target, RadioManager::instance().local_id())) {
        ArcanePacket a = {};
        a.type     = PacketType::ACK_DELIVERED;
        a.msgId    = p.msgId;
        a.hopCount = 3u;
        memcpy(a.sender, RadioManager::instance().local_id(), 6u);
        memcpy(a.target, p.sender, 6u);
        out_replies.push_back(a);
    }
}

} // namespace ogsm

