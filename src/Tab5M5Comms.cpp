/**
 * Only translation unit in this project that includes Arduino <WiFi.h> (C6 + SDIO hosted on Tab5).
 * Logic follows M5’s Tab5 Wi-Fi examples: WiFi.setPins(BOARD_SDIO_*) / STA / scan; see M5 Tab5 Wi-Fi docs.
 */
#include "Tab5M5Comms.hpp"
#include "app_config.h"
#include <Arduino.h>
#include "sdkconfig.h"
#include "soc/soc_caps.h"

#if defined(M5STACK_TAB5) && defined(CONFIG_IDF_TARGET_ESP32P4) && (SOC_WIFI_SUPPORTED || CONFIG_ESP_WIFI_REMOTE_ENABLED)
#include <WiFi.h>
#include <WiFiClient.h>
#if defined(ARDUINO) && __has_include(<pins_arduino.h>)
#include <pins_arduino.h>
#endif
#include "freertos/semphr.h"

static bool              s_bridge;
static SemaphoreHandle_t s_wifi_api_mtx;
static void              wifi_lock() {
    if (!s_wifi_api_mtx) {
        s_wifi_api_mtx = xSemaphoreCreateMutex();
    }
    (void)xSemaphoreTake(s_wifi_api_mtx, portMAX_DELAY);
}
static void wifi_unlock() {
    (void)xSemaphoreGive(s_wifi_api_mtx);
}

static void ensure_bridge() {
    if (s_bridge) {
        return;
    }
#if ARC_DEBUG_WIFI
    Serial.println(F("[arc:wifi] ensure_bridge: first init (setPins + mode STA)"));
    Serial.flush();
#endif
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
#if ARC_DEBUG_WIFI
    Serial.println(F("[arc:wifi] ensure_bridge: init done (s_bridge=1)"));
    Serial.flush();
#endif
}

bool M5Comms_::isReady() {
#if !(SOC_WIFI_SUPPORTED || CONFIG_ESP_WIFI_REMOTE_ENABLED)
    return false;
#else
    wifi_lock();
    ensure_bridge();
    const bool ok = s_bridge;
    wifi_unlock();
    return ok;
#endif
}

int M5Comms_::WiFi_::scanNetworks() {
    wifi_lock();
    ensure_bridge();
#if ARC_DEBUG_WIFI
    Serial.println(F("[arc:wifi] M5Comms::scanNetworks: before ::WiFi.scanNetworks"));
    Serial.flush();
#endif
    /* Longer active dwell per channel (default min 100ms) improves discovery on C6/remote Wi‑Fi. */
    int const n = ::WiFi.scanNetworks(false, true, false, 500, 0, NULL, NULL);
#if ARC_DEBUG_WIFI
    Serial.print(F("[arc:wifi] M5Comms::scanNetworks: after n="));
    Serial.println(n);
    Serial.flush();
#endif
    wifi_unlock();
    return n;
}

String M5Comms_::WiFi_::SSID(int index) {
    wifi_lock();
    ensure_bridge();
    String s = ::WiFi.SSID(index);
    wifi_unlock();
    return s;
}

int M5Comms_::WiFi_::RSSI(int index) {
    wifi_lock();
    ensure_bridge();
    const int r = (int)::WiFi.RSSI(index);
    wifi_unlock();
    return r;
}

int M5Comms_::WiFi_::channel(int index) {
    wifi_lock();
    ensure_bridge();
    const int c = (int)::WiFi.channel(index);
    wifi_unlock();
    return c;
}

void M5Comms_::WiFi_::scanDelete() {
    wifi_lock();
    ::WiFi.scanDelete();
    wifi_unlock();
}

int M5Comms_::WiFi_::encryptionType(int index) {
    wifi_lock();
    ensure_bridge();
    const int t = (int)::WiFi.encryptionType((uint8_t)index);
    wifi_unlock();
    return t;
}

int M5Comms_::WiFi_::beginConnect(const char *ssid, const char *pass) {
    wifi_lock();
    ensure_bridge();
    if (!ssid || !ssid[0]) {
        wifi_unlock();
        return (int)WL_DISCONNECTED;
    }
#if ARC_DEBUG_WIFI
    Serial.print(F("[arc:wifi] M5Comms::beginConnect: before WiFi.begin ssid="));
    Serial.println(ssid);
    Serial.flush();
#endif
    if (pass && pass[0]) {
        (void)::WiFi.begin(ssid, pass);
    } else {
        (void)::WiFi.begin(ssid, (const char *)NULL);
    }
#if ARC_DEBUG_WIFI
    Serial.print(F("[arc:wifi] M5Comms::beginConnect: after begin status="));
    Serial.println((int)::WiFi.status());
    Serial.flush();
#endif
    int const st = (int)::WiFi.status();
    wifi_unlock();
    return st;
}

void M5Comms_::WiFi_::disconnect(bool wifioff, bool erase_ap) {
    wifi_lock();
    ensure_bridge();
    (void)::WiFi.disconnect(wifioff, erase_ap);
    wifi_unlock();
}

int M5Comms_::WiFi_::status() {
    wifi_lock();
    (void)ensure_bridge();
    int const s = (int)::WiFi.status();
    wifi_unlock();
    return s;
}

String M5Comms_::WiFi_::connectedSSID() {
    wifi_lock();
    (void)ensure_bridge();
    String x = ::WiFi.SSID();
    wifi_unlock();
    return x;
}

int M5Comms_::WiFi_::connectedRSSI() {
    wifi_lock();
    (void)ensure_bridge();
    int const r = (int)::WiFi.RSSI();
    wifi_unlock();
    return r;
}

String M5Comms_::WiFi_::localIPString() {
    wifi_lock();
    (void)ensure_bridge();
    String x = ::WiFi.localIP().toString();
    wifi_unlock();
    return x;
}

bool M5Comms_::WiFi_::probeTcp(const char *host, uint16_t port, uint32_t timeout_ms) {
    wifi_lock();
    (void)ensure_bridge();
    if (!host || !host[0] || (int)::WiFi.status() != WL_CONNECTED) {
        wifi_unlock();
        return false;
    }
    WiFiClient c;
    c.setTimeout((int)timeout_ms);
    bool const ok = c.connect(host, port);
    if (ok) {
        c.stop();
    }
    wifi_unlock();
    return ok;
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

