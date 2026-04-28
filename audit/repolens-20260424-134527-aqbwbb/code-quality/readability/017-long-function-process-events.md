---
title: "[017] [MEDIUM] Long function with multiple concerns in PinManager::loadFromStorage"
severity: MEDIUM
domain: code-readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
In `components/cdc_core/src/PinManager.cpp:94-155`, the `loadFromStorage` function has 61 lines that perform multiple operations:

```cpp
bool PinManager::loadFromStorage() {
    hal::ISecureElement* se = hal::getSecureElementInstance();
    if (!se || !se->isSessionActive()) {
        LOG_W(TAG, "SE session not active");
        return false;
    }

    uint8_t data[STORAGE_SIZE];
    uint16_t actualLen = 0;

    hal::SeResult result = se->rmemRead(RMEM_SLOT_PIN, data, STORAGE_SIZE, &actualLen);
    if (result != hal::SeResult::OK || actualLen != STORAGE_SIZE || data[0] != MAGIC_V3) {
        LOG_D(TAG, "No valid PIN data (len=%d, magic=0x%02X)", actualLen, data[0]);
        return false;
    }

    size_t pos = 1;

    // Badge hash
    memcpy(badgeHash_, &data[pos], BADGE_HASH_SIZE);
    pos += BADGE_HASH_SIZE;

    // Badge retries
    badgeRetries_ = data[pos++];

    // KDF params (skip algorithm bytes, we know them)
    pos += 2;  // KDF algo + Hash algo

    // Iteration count (big endian)
    iterations_ = (data[pos] << 24) | (data[pos+1] << 16) | (data[pos+2] << 8) | data[pos+3];
    pos += 4;

    // Salts
    memcpy(pw1Salt_, &data[pos], SALT_SIZE);
    pos += SALT_SIZE;
    memcpy(pw3Salt_, &data[pos], SALT_SIZE);
    pos += SALT_SIZE;

    // Hashes
    memcpy(pw1Hash_, &data[pos], KDF_HASH_SIZE);
    pos += KDF_HASH_SIZE;
    memcpy(pw3Hash_, &data[pos], KDF_HASH_SIZE);
    pos += KDF_HASH_SIZE;

    // Retries
    pw1Retries_ = data[pos++];
    pw3Retries_ = data[pos++];

    // Check if badge PIN differs from default
    uint8_t defaultHash[BADGE_HASH_SIZE];
    computeBadgeHash(DEFAULT_BADGE_PIN, defaultHash);
    badgePinIsSet_ = !compareHash(badgeHash_, defaultHash, BADGE_HASH_SIZE);

    LOG_I(TAG, "Loaded PINs from R-Memory (Badge=%d, PW1=%d, PW3=%d retries)",
          badgeRetries_, pw1Retries_, pw3Retries_);
    return true;
}
```

## Impact
- **Single responsibility violation**: The function reads storage, deserializes data, AND computes derived state
- **Testing difficulty**: Hard to test the deserialization logic separately from I/O
- **Maintenance burden**: Adding a new field requires modifying this monolithic function

## Evidence
**File**: `components/cdc_core/src/PinManager.cpp:94-155`

The function:
1. Opens/validates secure element session
2. Reads raw bytes from storage
3. Validates magic/version
4. Deserializes 10+ fields
5. Computes derived state (badgePinIsSet_)

## Recommended Fix
Split into smaller, focused functions:

```cpp
// Helper struct for deserialization
struct LoadedPinData {
    uint8_t badgeHash[BADGE_HASH_SIZE];
    uint8_t badgeRetries;
    uint8_t iterations[4];
    uint8_t pw1Salt[SALT_SIZE];
    uint8_t pw3Salt[SALT_SIZE];
    uint8_t pw1Hash[KDF_HASH_SIZE];
    uint8_t pw3Hash[KDF_HASH_SIZE];
    uint8_t pw1Retries;
    uint8_t pw3Retries;
};

// Deserialization helper
static bool deserializePinData(const uint8_t* data, size_t len, LoadedPinData* out) {
    if (len < STORAGE_SIZE || data[0] != MAGIC_V3) {
        return false;
    }

    size_t pos = 1;
    memcpy(out->badgeHash, &data[pos], BADGE_HASH_SIZE);
    pos += BADGE_HASH_SIZE;
    out->badgeRetries = data[pos++];
    pos += 2;  // Skip KDF algo bytes
    memcpy(out->iterations, &data[pos], 4);
    pos += 4;
    memcpy(out->pw1Salt, &data[pos], SALT_SIZE);
    pos += SALT_SIZE;
    memcpy(out->pw3Salt, &data[pos], SALT_SIZE);
    pos += SALT_SIZE;
    memcpy(out->pw1Hash, &data[pos], KDF_HASH_SIZE);
    pos += KDF_HASH_SIZE;
    memcpy(out->pw3Hash, &data[pos], KDF_HASH_SIZE);
    pos += KDF_HASH_SIZE;
    out->pw1Retries = data[pos++];
    out->pw3Retries = data[pos++];
    return true;
}

// Main function - now clearer
bool PinManager::loadFromStorage() {
    hal::ISecureElement* se = hal::getSecureElementInstance();
    if (!se || !se->isSessionActive()) {
        LOG_W(TAG, "SE session not active");
        return false;
    }

    uint8_t data[STORAGE_SIZE];
    uint16_t actualLen = 0;
    hal::SeResult result = se->rmemRead(RMEM_SLOT_PIN, data, STORAGE_SIZE, &actualLen);
    if (result != hal::SeResult::OK || actualLen != STORAGE_SIZE) {
        LOG_D(TAG, "No valid PIN data (len=%d)", actualLen);
        return false;
    }

    LoadedPinData loaded;
    if (!deserializePinData(data, actualLen, &loaded)) {
        LOG_D(TAG, "Invalid PIN data format (magic=0x%02X)", data[0]);
        return false;
    }

    // Apply loaded data
    memcpy(badgeHash_, loaded.badgeHash, BADGE_HASH_SIZE);
    badgeRetries_ = loaded.badgeRetries;
    iterations_ = (loaded.iterations[0] << 24) | (loaded.iterations[1] << 16) |
                  (loaded.iterations[2] << 8) | loaded.iterations[3];
    memcpy(pw1Salt_, loaded.pw1Salt, SALT_SIZE);
    memcpy(pw3Salt_, loaded.pw3Salt, SALT_SIZE);
    memcpy(pw1Hash_, loaded.pw1Hash, KDF_HASH_SIZE);
    memcpy(pw3Hash_, loaded.pw3Hash, KDF_HASH_SIZE);
    pw1Retries_ = loaded.pw1Retries;
    pw3Retries_ = loaded.pw3Retries;

    // Compute derived state
    uint8_t defaultHash[BADGE_HASH_SIZE];
    computeBadgeHash(DEFAULT_BADGE_PIN, defaultHash);
    badgePinIsSet_ = !compareHash(badgeHash_, defaultHash, BADGE_HASH_SIZE);

    LOG_I(TAG, "Loaded PINs from R-Memory (Badge=%d, PW1=%d, PW3=%d retries)",
          badgeRetries_, pw1Retries_, pw3Retries_);
    return true;
}
```

## References
- [Clean Code - Robert C. Martin](https://www.amazon.com/Clean-Code-Handbook-Software-Craftsmanship/dp/0132350882)
- [C++ Core Guidelines - F.19: Make functions small and focused](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#F19-make-functions-small-and-focused)
