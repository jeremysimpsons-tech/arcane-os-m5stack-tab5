/**
 * OGSM application-layer authenticated encryption.
 *
 * AES-256-GCM:
 *   cipher = [nonce(12) || ct(N) || tag(16)]
 * Key is generated once and stored in NVS (namespace "ogsm", key "k32").
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ogsm {

bool   crypto_ensure_key();
void   crypto_key_fingerprint8(uint8_t out8[8]); /* first 8 bytes of SHA256(key) */
size_t crypto_max_ciphertext_size(size_t plain_len); /* includes nonce+tag */
bool   crypto_encrypt(const uint8_t *plain, size_t plen, std::vector<uint8_t> &out_ct);
bool   crypto_decrypt(const uint8_t *in, size_t in_len, std::vector<uint8_t> &out_plain);

} // namespace ogsm

