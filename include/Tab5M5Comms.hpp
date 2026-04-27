/**
 * Community Tab5 pattern: communicate with the ESP32-C6 (WiFi) from the P4 over SDIO using
 * Arduino WiFi in ONE module only, after M5.begin() has run (PMIC / IO expander for C6 power).
 *
 * Note: M5Unified does not expose `M5.Network` on the global `M5` object.
 * This repo provides a small facade `M5Network` (see `include/M5Network.hpp`) so app code can use
 * `M5Network.begin(); M5Network.WiFi.*` without including <WiFi.h>.
 */
#pragma once
#include <WString.h>

struct M5Comms_ {
    struct WiFi_ {
        int  scanNetworks();
        String SSID(int index);
        int    RSSI(int index);
        int    channel(int index);
        /** Per scan result: `WIFI_AUTH_OPEN` means no password. */
        int    encryptionType(int index);
        void   scanDelete();
        /** STA connect (non-blocking `begin`). `pass` null or empty for open AP. */
        int    beginConnect(const char *ssid, const char *pass);
        void   disconnect(bool wifioff, bool erase_ap);
        /** `wl_status_t` as int (e.g. `WL_CONNECTED`). */
        int    status();
        String connectedSSID();
        int    connectedRSSI();
        String localIPString();
        /** TCP connect test (e.g. 1.1.1.1:80); for “has internet” heuristic. */
        bool   probeTcp(const char *host, uint16_t port, uint32_t timeout_ms);
    } WiFi;
    /** True once SDIO pins + WiFi stack are up for the C6 (setPins + STA). */
    bool isReady();
    /** Bring up the hosted bridge (setPins + STA), without connecting. Safe after `M5.begin()`. */
    bool networkBegin();
    /** Hosted STA MAC address (6 bytes). */
    void sta_mac(uint8_t out[6]);

    /** ESP-NOW-like send/recv hook; on ESP32-P4 it is typically stubbed (no link symbols yet). */
    bool espnow_begin();
    int  espnow_pop(uint8_t *buf, int cap);
    bool espnow_send_bcast(const uint8_t *d, size_t n);
};

/** Global: hosted bridge API; prefer `M5Network` facade in app code. */
extern M5Comms_ M5Comms;
