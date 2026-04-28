---
title: "[022] [LOW] Duplicate PIN validation logic in PinManager"
severity: LOW
domain: code-readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
In `components/cdc_core/src/PinManager.cpp`, the PIN validation logic for `verifyBadgePin`, `verifyPW1`, and `verifyPW3` is nearly identical:

```cpp
// verifyBadgePin (lines 295-323)
bool PinManager::verifyBadgePin(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) init();
    if (isBadgeBlocked()) {
        LOG_W(TAG, "Badge PIN blocked");
        return false;
    }
    uint8_t inputHash[BADGE_HASH_SIZE];
    if (!computeBadgeHash(pin, inputHash)) return false;
    if (compareHash(badgeHash_, inputHash, BADGE_HASH_SIZE)) {
        resetBadgeRetries();
        lockoutActive_ = false;
        LOG_I(TAG, "Badge PIN verified");
        return true;
    }
    badgeRetries_--;
    saveToStorage();
    LOG_W(TAG, "Wrong badge PIN, %d retries left", badgeRetries_);
    if (badgeRetries_ == 0) {
        startLockout();
    }
    return false;
}

// verifyPW1 (lines 406-428) - Nearly identical
bool PinManager::verifyPW1(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) init();
    if (pw1Retries_ == 0) {
        LOG_W(TAG, "PW1 blocked");
        return false;
    }
    uint8_t inputHash[KDF_HASH_SIZE];
    if (!computeKdfHash(pin, pw1Salt_, inputHash)) return false;
    if (compareHash(pw1Hash_, inputHash, KDF_HASH_SIZE)) {
        resetPW1Retries();
        LOG_I(TAG, "PW1 verified");
        return true;
    }
    pw1Retries_--;
    saveToStorage();
    LOG_W(TAG, "Wrong PW1, %d retries left", pw1Retries_);
    return false;
}

// verifyPW3 (lines 507-529) - Nearly identical
bool PinManager::verifyPW3(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) init();
    if (pw3Retries_ == 0) {
        LOG_W(TAG, "PW3 blocked");
        return false;
    }
    uint8_t inputHash[KDF_HASH_SIZE];
    if (!computeKdfHash(pin, pw3Salt_, inputHash)) return false;
    if (compareHash(pw3Hash_, inputHash, KDF_HASH_SIZE)) {
        resetPW3Retries();
        LOG_I(TAG, "PW3 verified");
        return true;
    }
    pw3Retries_--;
    saveToStorage();
    LOG_W(TAG, "Wrong PW3, %d retries left", pw3Retries_);
    return false;
}
```

## Impact
- **Maintenance burden**: Changes to validation logic must be applied to 3 places
- **Inconsistency risk**: Bug fix in one function might be forgotten in others
- **Code bloat**: ~90 lines of nearly identical code

## Evidence
**File**: `components/cdc_core/src/PinManager.cpp:295-529`

The three functions differ only in:
- Hash computation method (`computeBadgeHash` vs `computeKdfHash`)
- Hash/salt member variables
- Retry counter names
- Log messages

## Recommended Fix
Extract a template or parameterized helper:

```cpp
// Helper struct for PIN verification
struct PinConfig {
    const char* name;
    size_t hashSize;
    uint8_t* storedHash;
    const uint8_t* salt;
    uint8_t* retries;
    bool (*computeHash)(const char*, const uint8_t*, uint8_t*);
    void (*resetRetries)();
    bool isBlocked();
};

template <typename Config>
bool PinManager::verifyPin(const char* pin, const Config& cfg) {
    if (!pin) return false;
    if (!pinLoaded_) init();
    if (cfg.isBlocked()) {
        LOG_W(TAG, "%s blocked", cfg.name);
        return false;
    }
    uint8_t inputHash[32];  // Max hash size
    if (!cfg.computeHash(pin, cfg.salt, inputHash)) return false;
    if (memcmp(cfg.storedHash, inputHash, cfg.hashSize) == 0) {
        cfg.resetRetries();
        LOG_I(TAG, "%s verified", cfg.name);
        return true;
    }
    (*cfg.retries)--;
    saveToStorage();
    LOG_W(TAG, "Wrong %s, %d retries left", cfg.name, *cfg.retries);
    return false;
}

// Usage
bool PinManager::verifyBadgePin(const char* pin) {
    return verifyPin(pin, PinConfig{
        .name = "Badge PIN",
        .hashSize = BADGE_HASH_SIZE,
        .storedHash = badgeHash_,
        .salt = nullptr,
        .retries = &badgeRetries_,
        .computeHash = [](const char* p, const uint8_t*, uint8_t* h) {
            return computeBadgeHash(p, h);
        },
        .resetRetries = [this]() { resetBadgeRetries(); lockoutActive_ = false; },
        .isBlocked = [this]() { return isBadgeBlocked(); }
    });
}
```

## References
- [DRY Principle](https://en.wikipedia.org/wiki/Don%27t_repeat_yourself)
- [C++ Core Guidelines - D.12: Use templates for generic code](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#d12-use-templates-for-generic-code)
