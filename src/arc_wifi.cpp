/**
 * Saved Wi‑Fi credentials: AES-128-ECB + PKCS#7, key = first 16 bytes of SHA256(salt || efuse MAC).
 */
#include "arc_wifi.hpp"
#include "app_config.h"
#include "Tab5M5Comms.hpp"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <string.h>
#include "mbedtls/aes.h"
#include "mbedtls/sha256.h"
#if defined(M5STACK_TAB5) && defined(CONFIG_IDF_TARGET_ESP32P4)
#include <WiFiType.h>
#endif

#define ARC_WIFI_NVS   "arc_net"
#define ARC_WIFI_MAGIC 0xAF21BEEFu

#if defined(M5STACK_TAB5) && defined(CONFIG_IDF_TARGET_ESP32P4)

static bool     s_inet        = false;
static bool     s_inet_valid  = false;
static uint32_t s_last_inet_ms;
static uint32_t s_last_poll_ms;

static void key16(uint8_t k[16]) {
    uint64_t     m   = ESP.getEfuseMac();
    uint8_t      mac[6];
    const char  *salt = "arc_wifi_v1";
    uint8_t      h[32];
    uint8_t      in[32];
    memcpy(mac, &m, 6);
    memcpy(in, salt, 11);
    memcpy(in + 11, mac, 6);
    (void)mbedtls_sha256(in, 17, h, 0);
    memcpy(k, h, 16);
}

static int pkcs7_pad(const uint8_t *in, int len, uint8_t *out, int outcap) {
    const int blk = 16;
    int       pad = blk - (len % blk);
    if (len + pad > outcap)
        return -1;
    memcpy(out, in, (size_t)len);
    for (int i = 0; i < pad; i++)
        out[len + i] = (uint8_t)pad;
    return len + pad;
}

static int pkcs7_unpad(uint8_t *buf, int len) {
    if (len <= 0 || (len % 16) != 0)
        return -1;
    uint8_t p = buf[len - 1];
    if (p == 0 || p > 16)
        return -1;
    for (int i = 0; i < p; i++) {
        if (buf[len - 1 - i] != p)
            return -1;
    }
    return len - p;
}

static bool hex_encode(const uint8_t *d, size_t len, char *hex, size_t hexcap) {
    if (hexcap < len * 2u + 1u)
        return false;
    static const char *H = "0123456789ABCDEF";
    for (size_t i = 0; i < len; i++) {
        hex[i * 2]     = H[(d[i] >> 4) & 0xF];
        hex[i * 2 + 1] = H[d[i] & 0xF];
    }
    hex[len * 2] = 0;
    return true;
}

static bool hex_decode(const char *hex, uint8_t *out, size_t outcap, size_t *outlen) {
    size_t n = strlen(hex);
    if (n % 2u != 0u || n / 2u > outcap)
        return false;
    auto ch = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return 10 + c - 'A';
        if (c >= 'a' && c <= 'f')
            return 10 + c - 'a';
        return -1;
    };
    for (size_t i = 0; i < n / 2; i++) {
        int hi = ch(hex[i * 2]);
        int lo = ch(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0)
            return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    *outlen = n / 2u;
    return true;
}

static bool crypto_encrypt(const char *plain, char *out_hex, size_t out_hex_cap) {
    if (!plain)
        return false;
    const int plen = (int)strnlen(plain, 64);
    uint8_t   key[16];
    uint8_t   buf[80];
    uint8_t   blk[16];
    key16(key);
    int tot = pkcs7_pad((const uint8_t *)plain, plen, buf, (int)sizeof(buf));
    if (tot < 0)
        return false;
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }
    for (int off = 0; off < tot; off += 16) {
        if (mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, buf + off, blk) != 0) {
            mbedtls_aes_free(&aes);
            return false;
        }
        memcpy(buf + off, blk, 16);
    }
    mbedtls_aes_free(&aes);
    return hex_encode(buf, (size_t)tot, out_hex, out_hex_cap);
}

static bool crypto_decrypt(const char *in_hex, char *out, size_t outcap) {
    uint8_t  raw[96];
    size_t   rlen = 0;
    if (!in_hex || !in_hex[0] || !hex_decode(in_hex, raw, sizeof(raw), &rlen))
        return false;
    if (rlen == 0 || (rlen % 16u) != 0u)
        return false;
    uint8_t key[16];
    uint8_t blk[16];
    key16(key);
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_dec(&aes, key, 128) != 0) {
        mbedtls_aes_free(&aes);
        return false;
    }
    for (size_t off = 0; off < rlen; off += 16) {
        if (mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, raw + off, blk) != 0) {
            mbedtls_aes_free(&aes);
            return false;
        }
        memcpy(raw + off, blk, 16);
    }
    mbedtls_aes_free(&aes);
    int dlen = pkcs7_unpad(raw, (int)rlen);
    if (dlen < 0 || (size_t)dlen >= outcap)
        return false;
    raw[(size_t)dlen] = 0;
    memcpy(out, raw, (size_t)dlen + 1u);
    return true;
}

bool arc_wifi_get_saved(char *ssid, size_t ssid_len, char *pass, size_t pass_len) {
    if (ssid && ssid_len)
        ssid[0] = 0;
    if (pass && pass_len)
        pass[0] = 0;
    Preferences p;
    if (!p.begin(ARC_WIFI_NVS, true))
        return false;
    if (p.getUInt("magic", 0) != ARC_WIFI_MAGIC) {
        p.end();
        return false;
    }
    String s = p.getString("s", "");
    String h = p.getString("p", "");
    p.end();
    if (s.isEmpty() || s.length() >= (int)ssid_len)
        return false;
    memcpy(ssid, s.c_str(), (size_t)s.length() + 1u);
    if (pass) {
        if (h.isEmpty()) {
            pass[0] = 0;
        } else {
            if (!crypto_decrypt(h.c_str(), pass, pass_len)) {
                ssid[0] = 0;
                return false;
            }
        }
    }
    return true;
}

void arc_wifi_save(const char *ssid, const char *pass) {
    if (!ssid || !ssid[0])
        return;
    char   hx[200];
    if (pass && pass[0]) {
        if (!crypto_encrypt(pass, hx, sizeof(hx)))
            return;
    } else
        hx[0] = 0;
    Preferences p;
    if (!p.begin(ARC_WIFI_NVS, false))
        return;
    p.putUInt("magic", ARC_WIFI_MAGIC);
    p.putString("s", ssid);
    p.putString("p", (pass && pass[0]) ? hx : "");
    p.end();
}

void arc_wifi_forget() {
    arc_net_invalidate();
    Preferences p;
    if (p.begin(ARC_WIFI_NVS, false)) {
        p.clear();
        p.end();
    }
    if (M5Comms.isReady()) {
        M5Comms.WiFi.disconnect(false, true);
    }
}

bool arc_wifi_is_saved(const char *ssid) {
    if (!ssid || !ssid[0])
        return false;
    char a[33], p[65];
    if (!arc_wifi_get_saved(a, sizeof a, p, sizeof p))
        return false;
    return strncmp(a, ssid, sizeof a) == 0;
}

void arc_net_invalidate() {
    s_inet_valid = false;
}

void arc_net_poll() {
    const uint32_t now = (uint32_t)millis();
    if (now - s_last_poll_ms < 400u) {
        return;
    }
    s_last_poll_ms = now;
    if (!M5Comms.isReady()) {
        s_inet = false;
        s_inet_valid = true;
        return;
    }
    if ((int)M5Comms.WiFi.status() != WL_CONNECTED) {
        s_inet = false;
        s_inet_valid = true;
        return;
    }
    if (s_inet_valid && (now - s_last_inet_ms < 12000u)) {
        return;
    }
    s_last_inet_ms = now;
#if ARC_DEBUG_WIFI
    Serial.println("[arc:wifi] arc_net_poll: before probeTcp(1.1.1.1:80)");
    Serial.flush();
#endif
    s_inet         = M5Comms.WiFi.probeTcp("1.1.1.1", 80, 2000u);
#if ARC_DEBUG_WIFI
    Serial.print("[arc:wifi] arc_net_poll: after probe ok=");
    Serial.println((int)s_inet);
    Serial.flush();
#endif
    s_inet_valid   = true;
}

bool arc_net_is_online() {
    if (!s_inet_valid) {
        arc_net_poll();
    }
    return s_inet;
}

#else /* !Tab5 P4 */

bool arc_wifi_get_saved(char *ssid, size_t ssid_len, char *pass, size_t pass_len) {
    (void)ssid;
    (void)ssid_len;
    (void)pass;
    (void)pass_len;
    return false;
}
void arc_wifi_save(const char *, const char *) {}
void arc_wifi_forget() {}
bool arc_wifi_is_saved(const char *) { return false; }
void arc_net_poll() {}
bool arc_net_is_online() { return false; }
void arc_net_invalidate() {}

#endif
