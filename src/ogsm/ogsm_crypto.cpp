#include "ogsm/ogsm_crypto.hpp"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_random.h>
#include <string.h>
#include "mbedtls/gcm.h"
#include "mbedtls/sha256.h"

namespace ogsm {

static uint8_t s_key[32];
static bool    s_have_key = false;

static bool load_or_make_key() {
    Preferences p;
    if (p.begin("ogsm", true)) {
        if (p.getBytesLength("k32") == 32) {
            p.getBytes("k32", s_key, 32u);
            p.end();
            s_have_key = true;
            return true;
        }
        p.end();
    }
    if (!p.begin("ogsm", false)) {
        return false;
    }
    esp_fill_random(s_key, 32u);
    p.putBytes("k32", s_key, 32u);
    p.end();
    s_have_key = true;
    return true;
}

bool crypto_ensure_key() {
    if (s_have_key) {
        return true;
    }
    return load_or_make_key();
}

void crypto_key_fingerprint8(uint8_t out8[8]) {
    if (!out8) {
        return;
    }
    (void)crypto_ensure_key();
    uint8_t h[32];
    (void)mbedtls_sha256(s_key, 32u, h, 0);
    memcpy(out8, h, 8u);
}

size_t crypto_max_ciphertext_size(size_t plain_len) {
    /* nonce(12) + ct(N) + tag(16) */
    return 12u + plain_len + 16u;
}

bool crypto_encrypt(const uint8_t *plain, size_t plen, std::vector<uint8_t> &out_ct) {
    if (!plain || plen == 0u) {
        return false;
    }
    if (!crypto_ensure_key()) {
        return false;
    }
    uint8_t nonce[12];
    esp_fill_random(nonce, sizeof nonce);
    out_ct.assign(12u + plen + 16u, 0);
    memcpy(out_ct.data(), nonce, 12u);

    mbedtls_gcm_context g;
    mbedtls_gcm_init(&g);
    if (mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, s_key, 256) != 0) {
        mbedtls_gcm_free(&g);
        return false;
    }
    uint8_t *ct  = out_ct.data() + 12u;
    uint8_t *tag = out_ct.data() + 12u + plen;
    const int rc = mbedtls_gcm_crypt_and_tag(&g, MBEDTLS_GCM_ENCRYPT, plen, nonce, sizeof nonce, nullptr, 0, plain, ct, 16u, tag);
    mbedtls_gcm_free(&g);
    return rc == 0;
}

bool crypto_decrypt(const uint8_t *in, size_t in_len, std::vector<uint8_t> &out_plain) {
    if (!in || in_len < 12u + 1u + 16u) {
        return false;
    }
    if (!crypto_ensure_key()) {
        return false;
    }
    const uint8_t *nonce = in;
    const size_t   clen  = in_len - 12u - 16u;
    const uint8_t *ct    = in + 12u;
    const uint8_t *tag   = in + 12u + clen;

    out_plain.assign(clen, 0);
    mbedtls_gcm_context g;
    mbedtls_gcm_init(&g);
    if (mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, s_key, 256) != 0) {
        mbedtls_gcm_free(&g);
        return false;
    }
    const int rc = mbedtls_gcm_auth_decrypt(&g, clen, nonce, 12u, nullptr, 0, tag, 16u, ct, out_plain.data());
    mbedtls_gcm_free(&g);
    return rc == 0;
}

} // namespace ogsm

