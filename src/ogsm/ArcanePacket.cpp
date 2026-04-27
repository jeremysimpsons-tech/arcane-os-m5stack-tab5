#include "ogsm/ArcanePacket.hpp"
#include <cstring>

namespace ogsm {

static constexpr uint8_t kWireVer = 1u;

size_t pack_plain(const ArcanePacket &p, uint8_t *out, size_t out_cap) {
    const size_t need = 1u + 4u + 6u + 6u + 1u + 1u + 1u + (size_t)p.payloadLen;
    if (!out || out_cap < need) {
        return 0u;
    }
    size_t o = 0u;
    out[o++] = kWireVer;
    memcpy(out + o, &p.msgId, 4u);
    o += 4u;
    memcpy(out + o, p.sender, 6u);
    o += 6u;
    memcpy(out + o, p.target, 6u);
    o += 6u;
    out[o++] = (uint8_t)p.type;
    out[o++] = p.hopCount;
    out[o++] = p.payloadLen;
    memcpy(out + o, p.payload, p.payloadLen);
    o += (size_t)p.payloadLen;
    return o;
}

bool unpack_plain(const uint8_t *in, size_t in_len, ArcanePacket &p) {
    if (!in || in_len < 1u + 4u + 6u + 6u + 1u + 1u + 1u) {
        return false;
    }
    size_t o = 0u;
    if (in[o++] != kWireVer) {
        return false;
    }
    memcpy(&p.msgId, in + o, 4u);
    o += 4u;
    memcpy(p.sender, in + o, 6u);
    o += 6u;
    memcpy(p.target, in + o, 6u);
    o += 6u;
    p.type     = (PacketType)in[o++];
    p.hopCount = in[o++];
    p.payloadLen = in[o++];
    if (p.payloadLen > ArcanePacket::kMaxPayload) {
        return false;
    }
    if (in_len < o + (size_t)p.payloadLen) {
        return false;
    }
    memcpy(p.payload, in + o, p.payloadLen);
    p.payload[p.payloadLen] = 0;
    return true;
}

} // namespace ogsm

