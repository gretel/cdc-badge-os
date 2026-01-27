#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KEY_FINGERPRINT_WORD_COUNT 5
#define KEY_FINGERPRINT_MAX_LEN 64

// Generiert alchemistischen Fingerprint für ECC Key
// slot: TROPIC01 ECC Slot (0-31)
// buf: Ausgabepuffer (min. 64 Bytes empfohlen)
// len: Puffergröße
// Rückgabe: true bei Erfolg
bool key_fingerprint_generate(uint8_t slot, char *buf, size_t len);

// Generiert Fingerprint aus rohem Public Key
bool key_fingerprint_from_pubkey(const uint8_t *pubkey, size_t pubkey_len,
                                  char *buf, size_t len);

// Liefert einzelnes Wort für Index (0-31)
const char* key_fingerprint_word(uint8_t index);

#ifdef __cplusplus
}
#endif
