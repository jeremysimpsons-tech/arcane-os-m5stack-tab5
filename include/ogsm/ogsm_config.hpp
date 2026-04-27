/**
 * OGSM transport defaults and compile-time notes.
 *
 * Tab5 (ESP32-P4 + ESP32-C6 hosted Wi-Fi): the bridge is brought up via `M5Network.begin()`, which
 * calls into `Tab5M5Comms` (the only translation unit that includes <WiFi.h>).
 *
 * ESP-NOW: On current ESP32-P4 Arduino remote Wi-Fi builds, `esp_now_*` symbols may not link even
 * if headers exist. `M5Comms.espnow_*` is therefore best-effort and may be stubbed; LoRa stays active.
 *
 * Encryption: ESP-NOW/LoRa are raw frames. SSL/TLS is not applicable; OGSM uses application-layer
 * authenticated encryption (AES-256-GCM).
 */
#pragma once
#include <cstdint>

namespace ogsm {

enum class RadioPath : uint8_t { EspNow = 1, LoRa = 2, Both = 3 };

struct RadioConfig {
    RadioPath prefer   = RadioPath::Both;
    uint8_t   mesh_ttl = 3;
};

/* Port A: UART pins are TX=37, RX=38 in `variants/m5stack_tab5/pins_arduino.h` (Serial2). */
#ifndef OGSM_LORA_BAUD
#define OGSM_LORA_BAUD 115200
#endif

/* Max air-frame size for LoRa UART framing (payload of the OGSM frame, including crypto header). */
#ifndef OGSM_LORA_MAX_FRAME
#define OGSM_LORA_MAX_FRAME 300
#endif

} // namespace ogsm

