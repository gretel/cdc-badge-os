#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KEY_FINGERPRINT_WORD_COUNT 5
#define KEY_FINGERPRINT_MAX_LEN 64

// Generate alchemical fingerprint for ECC key slot (0-31)
bool key_fingerprint_generate(uint8_t slot, char* buf, size_t len);

// Generate fingerprint from raw public key
bool key_fingerprint_from_pubkey(const uint8_t* pubkey, size_t pubkey_len,
                                 char* buf, size_t len);

// Lookup word for index (0-31)
const char* key_fingerprint_word(uint8_t index);

#ifdef __cplusplus
}
#endif

