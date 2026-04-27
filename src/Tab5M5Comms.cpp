/**
 * Only translation unit in this project that includes Arduino <WiFi.h> (C6 + SDIO hosted on Tab5).
 * Logic follows M5’s Tab5 Wi-Fi examples: WiFi.setPins(BOARD_SDIO_*) / STA / scan; see M5 Tab5 Wi-Fi docs.
 */
#include "Tab5M5Comms.hpp"
#include "M5Network.hpp"
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
#include <string.h>

/* esp_now headers exist for ESP32-P4 remote Wi-Fi builds but symbols are not in the link set yet. */
#if defined(CONFIG_IDF_TARGET_ESP32P4)
#define M5COMMS_HAS_ESP_NOW 0
#elif __has_include(<esp_now.h>)
#include <esp_now.h>
#include <esp_wifi.h>
#define M5COMMS_HAS_ESP_NOW 1
#endif
#ifndef M5COMMS_HAS_ESP_NOW
#define M5COMMS_HAS_ESP_NOW 0
#endif

#if M5COMMS_HAS_ESP_NOW
struct M5EspQ {
    uint8_t len;
    uint8_t data[250];
};
static QueueHandle_t s_espq = nullptr;
static void esp_now_rx_cb(const esp_now_recv_info_t *info, const uint8_t *data, int data_len) {
    (void)info;
    if (!s_espq || !data || data_len <= 0) {
        return;
    }
    M5EspQ m;
    if (data_len > (int)sizeof m.data) {
        data_len = (int)sizeof m.data;
    }
    m.len = (uint8_t)data_len;
    memcpy(m.data, data, (size_t)data_len);
    (void)xQueueSend(s_espq, &m, 0);
}
#endif

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

bool M5Comms_::networkBegin() {
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

void M5Comms_::sta_mac(uint8_t out[6]) {
    if (!out) {
        return;
    }
    memset(out, 0, 6u);
    wifi_lock();
    ensure_bridge();
    (void)::WiFi.macAddress(out);
    wifi_unlock();
}

#if M5COMMS_HAS_ESP_NOW
static bool s_espnow_inited = false;
bool        M5Comms_::espnow_begin() {
    (void)networkBegin();
    wifi_lock();
    ensure_bridge();
    if (!s_espq) {
        s_espq = xQueueCreate(24, sizeof(M5EspQ));
    }
    if (!s_espq) {
        wifi_unlock();
        return false;
    }
    const esp_err_t e = esp_now_init();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) {
        wifi_unlock();
        return false;
    }
    if (!s_espnow_inited) {
        s_espnow_inited = true;
        (void)esp_now_register_recv_cb(esp_now_rx_cb);
        esp_now_peer_info_t p;
        memset(&p, 0, sizeof p);
        const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        memcpy(p.peer_addr, bcast, 6u);
        p.channel = 0;
        p.ifidx   = WIFI_IF_STA;
        p.encrypt = false;
        (void)esp_now_add_peer(&p);
    }
    wifi_unlock();
    return true;
}

int M5Comms_::espnow_pop(uint8_t *buf, int cap) {
    if (!buf || cap <= 0 || !s_espq) {
        return 0;
    }
    M5EspQ m;
    if (xQueueReceive(s_espq, &m, 0) != pdTRUE) {
        return 0;
    }
    int n = (int)m.len;
    if (n > cap) {
        n = cap;
    }
    memcpy(buf, m.data, (size_t)n);
    return n;
}

bool M5Comms_::espnow_send_bcast(const uint8_t *d, size_t n) {
    if (!d || n == 0u || n > 250u) {
        return false;
    }
    const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    wifi_lock();
    ensure_bridge();
    const esp_err_t e = esp_now_send(bcast, const_cast<uint8_t *>(d), (int)n);
    wifi_unlock();
    return e == ESP_OK;
}
#else
bool M5Comms_::espnow_begin() {
    (void)networkBegin();
    return false;
}
int  M5Comms_::espnow_pop(uint8_t *, int) { return 0; }
bool M5Comms_::espnow_send_bcast(const uint8_t *, size_t) { return false; }
#endif

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
bool      M5Network_::begin() { return M5Comms.networkBegin(); }
M5Network_ M5Network{M5Comms.WiFi};

#else

bool M5Comms_::isReady() { return false; }
bool M5Comms_::networkBegin() { return false; }
void M5Comms_::sta_mac(uint8_t out[6]) {
    if (out) {
        memset(out, 0, 6u);
    }
}
bool M5Comms_::espnow_begin() { return false; }
int  M5Comms_::espnow_pop(uint8_t *, int) { return 0; }
bool M5Comms_::espnow_send_bcast(const uint8_t *, size_t) { return false; }
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
bool      M5Network_::begin() { return false; }
M5Network_ M5Network{M5Comms.WiFi};

#endif

