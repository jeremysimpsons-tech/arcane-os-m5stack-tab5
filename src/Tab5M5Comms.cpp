/**
 * Only translation unit in this project that includes Arduino <WiFi.h> (C6 + SDIO hosted on Tab5).
 * Logic follows M5’s Tab5 Wi-Fi examples: WiFi.setPins(BOARD_SDIO_*) / STA / scan; see M5 Tab5 Wi-Fi docs.
 */
#include "Tab5M5Comms.hpp"
#include <Arduino.h>
#include "sdkconfig.h"
#include "soc/soc_caps.h"

#if defined(M5STACK_TAB5) && defined(CONFIG_IDF_TARGET_ESP32P4) && (SOC_WIFI_SUPPORTED || CONFIG_ESP_WIFI_REMOTE_ENABLED)
#include <WiFi.h>
#include <WiFiClient.h>
#if defined(ARDUINO) && __has_include(<pins_arduino.h>)
#include <pins_arduino.h>
#endif

static bool s_bridge;

static void ensure_bridge() {
    if (s_bridge) {
        return;
    }
    /* C6 (esp-hosted) SDIO mapping:
     *  - M5Stack Tab5 retail: espressif/arduino-esp32 variants/m5stack_tab5/pins_arduino.h
     *  - esp32-p4-function-evboard: different pins; only if not building for Tab5. */
#if defined(BOARD_HAS_SDIO_ESP_HOSTED)
    (void)::WiFi.setPins(BOARD_SDIO_ESP_HOSTED_CLK, BOARD_SDIO_ESP_HOSTED_CMD, BOARD_SDIO_ESP_HOSTED_D0, BOARD_SDIO_ESP_HOSTED_D1, BOARD_SDIO_ESP_HOSTED_D2, BOARD_SDIO_ESP_HOSTED_D3, BOARD_SDIO_ESP_HOSTED_RESET);
#elif defined(M5STACK_TAB5)
    (void)::WiFi.setPins(12, 13, 11, 10, 9, 8, 15);
#else
    (void)::WiFi.setPins(18, 19, 14, 15, 16, 17, 54);
#endif
    (void)::WiFi.mode(WIFI_MODE_STA);
    ::WiFi.setSleep(false);
    s_bridge = true;
}

bool M5Comms_::isReady() {
#if !(SOC_WIFI_SUPPORTED || CONFIG_ESP_WIFI_REMOTE_ENABLED)
    return false;
#else
    ensure_bridge();
    return s_bridge;
#endif
}

int M5Comms_::WiFi_::scanNetworks() {
    ensure_bridge();
    /* Longer active dwell per channel (default min 100ms) improves discovery on C6/remote Wi‑Fi. */
    return ::WiFi.scanNetworks(false, true, false, 500, 0, NULL, NULL);
}

String M5Comms_::WiFi_::SSID(int index) {
    ensure_bridge();
    return ::WiFi.SSID(index);
}

int M5Comms_::WiFi_::RSSI(int index) {
    ensure_bridge();
    return (int)::WiFi.RSSI(index);
}

int M5Comms_::WiFi_::channel(int index) {
    ensure_bridge();
    return (int)::WiFi.channel(index);
}

void M5Comms_::WiFi_::scanDelete() {
    ::WiFi.scanDelete();
}

int M5Comms_::WiFi_::encryptionType(int index) {
    ensure_bridge();
    return (int)::WiFi.encryptionType((uint8_t)index);
}

int M5Comms_::WiFi_::beginConnect(const char *ssid, const char *pass) {
    ensure_bridge();
    if (!ssid || !ssid[0]) {
        return (int)WL_DISCONNECTED;
    }
    if (pass && pass[0]) {
        (void)::WiFi.begin(ssid, pass);
    } else {
        (void)::WiFi.begin(ssid, (const char *)NULL);
    }
    return (int)::WiFi.status();
}

void M5Comms_::WiFi_::disconnect(bool wifioff, bool erase_ap) {
    ensure_bridge();
    (void)::WiFi.disconnect(wifioff, erase_ap);
}

int M5Comms_::WiFi_::status() {
    (void)ensure_bridge();
    return (int)::WiFi.status();
}

String M5Comms_::WiFi_::connectedSSID() {
    (void)ensure_bridge();
    return ::WiFi.SSID();
}

int M5Comms_::WiFi_::connectedRSSI() {
    (void)ensure_bridge();
    return (int)::WiFi.RSSI();
}

String M5Comms_::WiFi_::localIPString() {
    (void)ensure_bridge();
    return ::WiFi.localIP().toString();
}

bool M5Comms_::WiFi_::probeTcp(const char *host, uint16_t port, uint32_t timeout_ms) {
    (void)ensure_bridge();
    if (!host || !host[0] || (int)::WiFi.status() != WL_CONNECTED) {
        return false;
    }
    WiFiClient c;
    c.setTimeout((int)timeout_ms);
    if (!c.connect(host, port)) {
        return false;
    }
    c.stop();
    return true;
}

M5Comms_ M5Comms;

#else

bool M5Comms_::isReady() { return false; }
int  M5Comms_::WiFi_::scanNetworks() { return -1; }
String M5Comms_::WiFi_::SSID(int) { return String(); }
int    M5Comms_::WiFi_::RSSI(int) { return 0; }
int    M5Comms_::WiFi_::channel(int) { return 0; }
void   M5Comms_::WiFi_::scanDelete() {}
int    M5Comms_::WiFi_::encryptionType(int) { return -1; }
int    M5Comms_::WiFi_::beginConnect(const char *, const char *) { return -1; }
void   M5Comms_::WiFi_::disconnect(bool, bool) {}
int    M5Comms_::WiFi_::status() { return -1; }
String M5Comms_::WiFi_::connectedSSID() { return String(); }
int    M5Comms_::WiFi_::connectedRSSI() { return 0; }
String M5Comms_::WiFi_::localIPString() { return String(); }
bool   M5Comms_::WiFi_::probeTcp(const char *, uint16_t, uint32_t) { return false; }
M5Comms_ M5Comms;

#endif

