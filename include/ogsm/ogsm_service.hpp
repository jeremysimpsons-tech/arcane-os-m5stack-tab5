#pragma once

#include "AppTypes.hpp"
#include "ogsm/ArcanePacket.hpp"
#include "ogsm/ogsm_config.hpp"
#include <cstdint>
#include <vector>

namespace ogsm {

enum class MsgStatus : uint8_t { Sending = 0, NextHop = 1, Delivered = 2, Read = 3 };

struct ChatLine {
    uint32_t  msgId = 0;
    bool      mine  = true;
    bool      unread = false;
    MsgStatus status = MsgStatus::Sending;
    uint8_t   peer[6] = {0}; /* other endpoint for this message */
    char      text[ArcanePacket::kMaxPayload] = {0};
};

void service_init();
void service_poll();

/* Hook from app_show(): manages unread state + ACK_READ */
void on_app_shown(AppScreen s);

unsigned unread_count();

void get_lines(std::vector<ChatLine> &out);
bool send_text(const char *utf8, const uint8_t target[6], RadioPath path);

/* Called by RadioManager on decrypted packet; may push replies. */
void on_rx_packet(const ArcanePacket &p, int link, std::vector<ArcanePacket> &out_replies);

} // namespace ogsm

