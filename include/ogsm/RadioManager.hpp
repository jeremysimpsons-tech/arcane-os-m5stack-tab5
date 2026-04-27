/**
 * RadioManager: protocol-agnostic send/receive for OGSM.
 *
 * - ESP-NOW path: via Tab5 hosted bridge (`M5Network.begin()` + `M5Comms.espnow_*`).
 * - LoRa path: UART `Serial2` (Port A) framing.
 *
 * UI/service call `sendMessage()` without knowing which radio is used.
 */
#pragma once

#include "ogsm/ArcanePacket.hpp"
#include "ogsm/ogsm_config.hpp"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace ogsm {

class RadioManager {
public:
    static RadioManager &instance();

    bool init(const RadioConfig &rc);
    void set_config(const RadioConfig &rc) { m_cfg = rc; }

    bool sendMessage(const ArcanePacket &p, RadioPath path_override);

    /** Pump receive queues for both radios; mesh relay + deliver-to-service. */
    void poll();

    const uint8_t *local_id() const { return m_local; }
    bool           espnow_ok() const { return m_esp_ok; }
    bool           lora_ok() const { return m_lora_ok; }

private:
    RadioManager() = default;

    bool pack_encrypt_air_(const ArcanePacket &in, std::vector<uint8_t> &out_air) const;
    bool unpack_decrypt_air_(const uint8_t *air, size_t n, ArcanePacket &out) const;
    bool send_air_(const std::vector<uint8_t> &air, RadioPath p);

    void handle_rx_air_(const uint8_t *air, size_t n, int link /*0=espnow 1=lora*/);
    void lora_rx_poll_();
    void esp_rx_poll_();
    void flush_pending_();
    void flush_relay_();

    bool seen_(uint32_t id) const;
    void remember_(uint32_t id);

    RadioConfig m_cfg = {};
    uint8_t     m_local[6] = {0};
    bool        m_inited = false;
    bool        m_esp_ok = false;
    bool        m_lora_ok = false;

    static constexpr int kDedupN = 96;
    uint32_t m_dedup[kDedupN] = {0};
    int      m_dedup_i = 0;

    std::vector<uint8_t>  m_lora_rx;
    std::deque<ArcanePacket> m_pending;

    struct RelayJob {
        uint32_t   due_ms = 0;
        ArcanePacket pkt  = {};
    };
    std::deque<RelayJob> m_relay;
    /* Relay storm protection: token bucket (max relays per second). */
    uint32_t m_rl_last_ms = 0;
    uint8_t  m_rl_tokens  = 0;
};

} // namespace ogsm

