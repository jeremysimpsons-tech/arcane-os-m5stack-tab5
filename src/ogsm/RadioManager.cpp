#include "ogsm/RadioManager.hpp"
#include "ogsm/ogsm_crypto.hpp"
#include "ogsm/ogsm_service.hpp"
#include "M5Network.hpp"
#include "Tab5M5Comms.hpp"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <string.h>
#if __has_include(<pins_arduino.h>)
#include <pins_arduino.h>
#endif

namespace ogsm {

static constexpr uint8_t kAirMagic = 0xA1u; /* payload is AEAD blob */
static const uint8_t     kBcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static bool id_eq(const uint8_t a[6], const uint8_t b[6]) { return memcmp(a, b, 6) == 0; }

static bool should_relay_chat(const ArcanePacket &p, const uint8_t me[6]) {
    if (p.type != PacketType::CHAT) return false;
    if (p.hopCount == 0u) return false;
    /* Allow broadcast relays too, but protect with jitter + rate limiting. */
    if (id_eq(p.sender, me)) return false;
    return !id_eq(p.target, me);
}

static void relay_tokens_refill(uint32_t now_ms, uint32_t &last_ms, uint8_t &tokens, uint8_t max_per_s) {
    if (last_ms == 0u) {
        last_ms = now_ms;
        tokens  = max_per_s;
        return;
    }
    if (now_ms - last_ms >= 1000u) {
        last_ms = now_ms;
        tokens  = max_per_s;
    }
}

static uint16_t crc16_ccitt(const uint8_t *d, size_t n) {
    uint16_t c = 0xFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= (uint16_t)d[i] << 8u;
        for (int b = 0; b < 8; b++) {
            c = (c & 0x8000u) ? (uint16_t)((c << 1u) ^ 0x1021u) : (uint16_t)(c << 1u);
        }
    }
    return c;
}

static void lora_write_frame(HardwareSerial &ser, const uint8_t *frame, size_t flen) {
    /* Frame: 'O''G' + lenLE + payload + crc16( header+len+payload ) */
    const uint8_t hdr[2] = {0x4F, 0x47};
    uint8_t       lenb[2] = {(uint8_t)(flen & 0xFFu), (uint8_t)(flen >> 8u)};
    std::vector<uint8_t> tmp;
    tmp.reserve(4u + flen);
    tmp.insert(tmp.end(), hdr, hdr + 2u);
    tmp.insert(tmp.end(), lenb, lenb + 2u);
    if (flen) tmp.insert(tmp.end(), frame, frame + flen);
    const uint16_t c = crc16_ccitt(tmp.data(), tmp.size());
    ser.write(tmp.data(), tmp.size());
    uint8_t cr[2] = {(uint8_t)(c & 0xFFu), (uint8_t)(c >> 8u)};
    ser.write(cr, 2u);
}

RadioManager &RadioManager::instance() {
    static RadioManager g;
    return g;
}

bool RadioManager::seen_(uint32_t id) const {
    for (int i = 0; i < kDedupN; i++) {
        if (m_dedup[i] == id) return true;
    }
    return false;
}

void RadioManager::remember_(uint32_t id) {
    m_dedup[m_dedup_i % kDedupN] = id;
    m_dedup_i++;
}

bool RadioManager::pack_encrypt_air_(const ArcanePacket &in, std::vector<uint8_t> &out_air) const {
    uint8_t plain[1 + 256];
    const size_t n = pack_plain(in, plain, sizeof plain);
    if (n == 0u) return false;
    std::vector<uint8_t> ct;
    if (!crypto_encrypt(plain, n, ct)) return false;
    out_air.clear();
    out_air.reserve(1u + ct.size());
    out_air.push_back(kAirMagic);
    out_air.insert(out_air.end(), ct.begin(), ct.end());
    return true;
}

bool RadioManager::unpack_decrypt_air_(const uint8_t *air, size_t n, ArcanePacket &out) const {
    if (!air || n < 2u || air[0] != kAirMagic) return false;
    std::vector<uint8_t> dec;
    if (!crypto_decrypt(air + 1u, n - 1u, dec)) return false;
    return unpack_plain(dec.data(), dec.size(), out);
}

bool RadioManager::send_air_(const std::vector<uint8_t> &air, RadioPath p) {
    bool ok_esp = false;
    bool ok_lor = false;
    if ((p == RadioPath::EspNow || p == RadioPath::Both) && m_esp_ok) {
        ok_esp = M5Comms.espnow_send_bcast(air.data(), air.size());
    }
    if ((p == RadioPath::LoRa || p == RadioPath::Both) && m_lora_ok) {
        lora_write_frame(Serial2, air.data(), air.size());
        ok_lor = true;
    }
    if (p == RadioPath::Both) return ok_esp || ok_lor;
    if (p == RadioPath::EspNow) return ok_esp;
    return ok_lor;
}

bool RadioManager::sendMessage(const ArcanePacket &p, RadioPath path_override) {
    const RadioPath use = (path_override == RadioPath::Both) ? m_cfg.prefer : path_override;
    std::vector<uint8_t> air;
    if (!pack_encrypt_air_(p, air)) return false;
    return send_air_(air, use);
}

void RadioManager::handle_rx_air_(const uint8_t *air, size_t n, int link) {
    ArcanePacket p = {};
    if (!unpack_decrypt_air_(air, n, p)) return;
    if (seen_(p.msgId)) return;
    remember_(p.msgId);

    /* Mesh relay for new messages not targeted to us. */
    if (should_relay_chat(p, m_local)) {
        ArcanePacket rel = p;
        if (rel.hopCount > 0u) rel.hopCount--;
        RelayJob j;
        j.due_ms = (uint32_t)millis() + (uint32_t)random(30, 150);
        j.pkt    = rel;
        m_relay.push_back(j);
    }

    /* Deliver to service only if target is us or broadcast. */
    if (!id_is_broadcast(p.target) && !id_eq(p.target, m_local)) {
        return;
    }
    std::vector<ArcanePacket> replies;
    ogsm::on_rx_packet(p, link, replies);
    for (const ArcanePacket &r : replies) {
        m_pending.push_back(r);
    }
}

void RadioManager::flush_pending_() {
    while (!m_pending.empty()) {
        const ArcanePacket p = m_pending.front();
        m_pending.pop_front();
        (void)sendMessage(p, m_cfg.prefer);
    }
}

void RadioManager::flush_relay_() {
    const uint8_t  kMaxPerSec = 3u;
    const uint32_t now        = (uint32_t)millis();
    relay_tokens_refill(now, m_rl_last_ms, m_rl_tokens, kMaxPerSec);
    while (!m_relay.empty()) {
        if (m_relay.front().due_ms > now) {
            return;
        }
        RelayJob j = m_relay.front();
        m_relay.pop_front();
        relay_tokens_refill(now, m_rl_last_ms, m_rl_tokens, kMaxPerSec);
        if (m_rl_tokens == 0u) {
            continue;
        }
        m_rl_tokens--;
        (void)sendMessage(j.pkt, m_cfg.prefer);
    }
}

void RadioManager::esp_rx_poll_() {
    if (!m_esp_ok) return;
    uint8_t b[260];
    for (;;) {
        const int n = M5Comms.espnow_pop(b, (int)sizeof b);
        if (n <= 0) break;
        handle_rx_air_(b, (size_t)n, 0);
    }
}

void RadioManager::lora_rx_poll_() {
    if (!m_lora_ok) return;
    while (Serial2.available() > 0) {
        const int c = Serial2.read();
        if (c < 0) break;
        m_lora_rx.push_back((uint8_t)c);
        if (m_lora_rx.size() > 1200u) {
            m_lora_rx.erase(m_lora_rx.begin(), m_lora_rx.begin() + 600);
        }
    }
    for (;;) {
        if (m_lora_rx.size() < 2u) return;
        size_t o = 0u;
        while (o + 1u < m_lora_rx.size() && (m_lora_rx[o] != 0x4F || m_lora_rx[o + 1u] != 0x47)) o++;
        if (o + 1u >= m_lora_rx.size()) {
            if (o) m_lora_rx.erase(m_lora_rx.begin(), m_lora_rx.begin() + o);
            return;
        }
        if (m_lora_rx.size() < o + 4u) return;
        const size_t len = (size_t)m_lora_rx[o + 2u] | ((size_t)m_lora_rx[o + 3u] << 8u);
        if (len > (size_t)OGSM_LORA_MAX_FRAME) {
            m_lora_rx.erase(m_lora_rx.begin(), m_lora_rx.begin() + (ptrdiff_t)(o + 1u));
            continue;
        }
        const size_t need = o + 4u + len + 2u;
        if (m_lora_rx.size() < need) {
            if (o) m_lora_rx.erase(m_lora_rx.begin(), m_lora_rx.begin() + o);
            return;
        }
        const uint8_t *blk = m_lora_rx.data() + o;
        const uint16_t cc  = crc16_ccitt(blk, 4u + len);
        const uint16_t gi  = (uint16_t)blk[4u + len] | ((uint16_t)blk[4u + len + 1u] << 8u);
        if (cc != gi) {
            m_lora_rx.erase(m_lora_rx.begin(), m_lora_rx.begin() + (ptrdiff_t)(o + 1u));
            continue;
        }
        const uint8_t *frame = blk + 4u;
        if (len) handle_rx_air_(frame, len, 1);
        m_lora_rx.erase(m_lora_rx.begin(), m_lora_rx.begin() + (ptrdiff_t)need);
    }
}

void RadioManager::poll() {
    if (!m_inited) return;
    esp_rx_poll_();
    lora_rx_poll_();
    flush_relay_();
    flush_pending_();
}

bool RadioManager::init(const RadioConfig &rc) {
    m_cfg = rc;
    m_inited = true;
    m_dedup_i = 0;
    memset(m_dedup, 0, sizeof m_dedup);
    m_pending.clear();
    m_relay.clear();
    m_lora_rx.clear();
    m_rl_last_ms = (uint32_t)millis();
    m_rl_tokens  = 3u;

    /* Local node id: use hosted STA MAC. */
    (void)M5Network.begin();
    M5Comms.sta_mac(m_local);

    /* ESP-NOW path (best-effort). */
    m_esp_ok = false;
    if (m_cfg.prefer == RadioPath::EspNow || m_cfg.prefer == RadioPath::Both) {
        (void)M5Network.begin();
        m_esp_ok = M5Comms.espnow_begin();
    }

    /* LoRa path: Serial2 (Port A). */
    m_lora_ok = false;
#if defined(RX) && defined(TX)
    const int p_rx = (int)RX;
    const int p_tx = (int)TX;
#else
    const int p_rx = 38;
    const int p_tx = 37;
#endif
    Serial2.end();
    Serial2.setRxBufferSize(1024);
    Serial2.begin(OGSM_LORA_BAUD, SERIAL_8N1, p_rx, p_tx);
    delay(20);
    m_lora_ok = true;

    return m_esp_ok || m_lora_ok;
}

} // namespace ogsm

