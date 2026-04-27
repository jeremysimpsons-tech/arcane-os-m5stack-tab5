/**
 * OGSM logical packet (encrypted on-air).
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ogsm {

enum class PacketType : uint8_t {
    CHAT          = 1,
    ACK_DELIVERED = 2,
    ACK_READ      = 3,
    HEARTBEAT     = 4,
};

inline bool id_is_broadcast(const uint8_t id[6]) {
    for (int i = 0; i < 6; i++) {
        if (id[i] != 0xFFu) {
            return false;
        }
    }
    return true;
}

struct ArcanePacket {
    uint32_t   msgId      = 0;
    uint8_t    sender[6]  = {0};
    uint8_t    target[6]  = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    PacketType type       = PacketType::CHAT;
    uint8_t    hopCount   = 3;
    uint8_t    payloadLen = 0;
    static constexpr size_t kMaxPayload = 160;
    char       payload[kMaxPayload] = {0};

    void set_payload_text(const char *utf8) {
        if (!utf8) {
            payloadLen = 0;
            payload[0] = 0;
            return;
        }
        size_t n = strnlen(utf8, kMaxPayload);
        payloadLen = (uint8_t)(n < kMaxPayload ? n : (kMaxPayload - 1));
        memcpy(payload, utf8, payloadLen);
        payload[payloadLen] = 0;
    }
};

/* Plain serialization used before AEAD encrypt. */
size_t pack_plain(const ArcanePacket &p, uint8_t *out, size_t out_cap);
bool   unpack_plain(const uint8_t *in, size_t in_len, ArcanePacket &p);

} // namespace ogsm

