/**
 * Community Tab5 pattern: communicate with the ESP32-C6 (WiFi) from the P4 over SDIO using
 * Arduino WiFi in ONE module only, after M5.begin() has run (PMIC / IO expander for C6 power).
 *
 * Your sketch used `M5.Network.WiFi.*` — M5Unified 0.2.x has no M5.Network; this mirrors that API
 * as `M5Comms` so the rest of the app never includes <WiFi.h>.
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
};

/** Global, same role as the community `M5.Network` helper. */
extern M5Comms_ M5Comms;
