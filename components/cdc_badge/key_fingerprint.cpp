#include "key_fingerprint.h"
#include "tropic01.h"
#include "mbedtls/sha256.h"
#include <string.h>
#include <stdio.h>

// 32 alchemistische Elemente (5 Bit pro Index)
static const char* const ALCHEMY_WORDS[32] = {
    // 4 Klassische Elemente
    "Fire",        // 0
    "Water",       // 1
    "Earth",       // 2
    "Air",         // 3
    // 5. Element
    "Aether",      // 4
    // Tria Prima
    "Sulfur",      // 5
    "Mercury",     // 6
    "Salt",        // 7
    // Planetenmetalle
    "Gold",        // 8
    "Silver",      // 9
    "Copper",      // 10
    "Iron",        // 11
    "Tin",         // 12
    "Lead",        // 13
    // Mundane Elemente
    "Antimony",    // 14
    "Arsenic",     // 15
    "Bismuth",     // 16
    "Phosphorus",  // 17
    "Platinum",    // 18
    "Zinc",        // 19
    "Magnesium",   // 20
    "Potassium",   // 21
    // Alchemistische Substanzen
    "Vitriol",     // 22
    "Aquafortis",  // 23
    "Alkahest",    // 24
    "Azoth",       // 25
    "Cinnabar",    // 26
    "Nitre",       // 27
    "Calx",        // 28
    "Regulus",     // 29
    // Philosophische Konzepte
    "Quintessence",// 30
    "Stone"        // 31
};

const char* key_fingerprint_word(uint8_t index) {
    if (index >= 32) return "?";
    return ALCHEMY_WORDS[index];
}

bool key_fingerprint_from_pubkey(const uint8_t *pubkey, size_t pubkey_len,
                                  char *buf, size_t len) {
    if (!pubkey || !buf || len < KEY_FINGERPRINT_MAX_LEN || pubkey_len == 0) {
        return false;
    }

    // SHA-256 Hash des Public Keys
    uint8_t hash[32];
    mbedtls_sha256(pubkey, pubkey_len, hash, 0);

    // 5 Wörter extrahieren (je 5 Bit = Index 0-31)
    // Verwendet die ersten 25 Bit des Hashes
    uint8_t indices[KEY_FINGERPRINT_WORD_COUNT];
    indices[0] = (hash[0] >> 3) & 0x1F;                          // Bits 0-4
    indices[1] = ((hash[0] << 2) | (hash[1] >> 6)) & 0x1F;       // Bits 5-9
    indices[2] = (hash[1] >> 1) & 0x1F;                          // Bits 10-14
    indices[3] = ((hash[1] << 4) | (hash[2] >> 4)) & 0x1F;       // Bits 15-19
    indices[4] = ((hash[2] << 1) | (hash[3] >> 7)) & 0x1F;       // Bits 20-24

    // Fingerprint-String zusammenbauen
    buf[0] = '\0';
    for (int i = 0; i < KEY_FINGERPRINT_WORD_COUNT; i++) {
        if (i > 0) {
            strlcat(buf, " ", len);
        }
        strlcat(buf, ALCHEMY_WORDS[indices[i]], len);
    }

    return true;
}

bool key_fingerprint_generate(uint8_t slot, char *buf, size_t len) {
    if (!buf || len < KEY_FINGERPRINT_MAX_LEN) {
        return false;
    }

    // Public Key aus TROPIC01 lesen
    uint8_t pubkey[64];
    uint8_t pubkey_len = sizeof(pubkey);

    if (!tropic01_ecc_key_read(slot, pubkey, pubkey_len, NULL, NULL)) {
        snprintf(buf, len, "(no key)");
        return false;
    }

    return key_fingerprint_from_pubkey(pubkey, pubkey_len, buf, len);
}
