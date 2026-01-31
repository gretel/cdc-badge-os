// Alchemical key fingerprints (FIDO/OpenPGP shared)

#include "cdc_core/KeyFingerprint.h"
#include "cdc_hal/ISecureElement.h"
#include "mbedtls/sha256.h"
#include <string.h>

// 32 alchemical elements (5-bit index)
static const char* const ALCHEMY_WORDS[32] = {
    "Fire",        "Water",      "Earth",      "Air",
    "Aether",      "Sulfur",     "Mercury",    "Salt",
    "Gold",        "Silver",     "Copper",     "Iron",
    "Tin",         "Lead",       "Antimony",   "Arsenic",
    "Bismuth",     "Phosphorus", "Platinum",   "Zinc",
    "Magnesium",   "Potassium",  "Vitriol",    "Aquafortis",
    "Alkahest",    "Azoth",      "Cinnabar",   "Nitre",
    "Calx",        "Regulus",    "Quintessence","Stone"
};

const char* key_fingerprint_word(uint8_t index) {
    if (index >= 32) return "?";
    return ALCHEMY_WORDS[index];
}

bool key_fingerprint_from_pubkey(const uint8_t* pubkey, size_t pubkey_len,
                                 char* buf, size_t len) {
    if (!pubkey || !buf || len < KEY_FINGERPRINT_MAX_LEN || pubkey_len == 0) {
        return false;
    }

    uint8_t hash[32];
    mbedtls_sha256(pubkey, pubkey_len, hash, 0);

    uint8_t indices[KEY_FINGERPRINT_WORD_COUNT];
    indices[0] = (hash[0] >> 3) & 0x1F;
    indices[1] = ((hash[0] << 2) | (hash[1] >> 6)) & 0x1F;
    indices[2] = (hash[1] >> 1) & 0x1F;
    indices[3] = ((hash[1] << 4) | (hash[2] >> 4)) & 0x1F;
    indices[4] = ((hash[2] << 1) | (hash[3] >> 7)) & 0x1F;

    buf[0] = '\0';
    for (int i = 0; i < KEY_FINGERPRINT_WORD_COUNT; i++) {
        if (i > 0) {
            strlcat(buf, " ", len);
        }
        strlcat(buf, ALCHEMY_WORDS[indices[i]], len);
    }

    return true;
}

bool key_fingerprint_generate(uint8_t slot, char* buf, size_t len) {
    if (!buf || len < KEY_FINGERPRINT_MAX_LEN) {
        return false;
    }

    auto* se = cdc::hal::getSecureElementInstance();
    if (!se) {
        return false;
    }

    uint8_t pubkey[64] = {};
    cdc::hal::EccCurve curve = cdc::hal::EccCurve::P256;
    if (se->eccGetPublicKey(slot, pubkey, &curve) != cdc::hal::SeResult::OK) {
        strlcpy(buf, "(no key)", len);
        return false;
    }

    size_t pubkey_len = (curve == cdc::hal::EccCurve::ED25519) ? 32 : 64;
    return key_fingerprint_from_pubkey(pubkey, pubkey_len, buf, len);
}

