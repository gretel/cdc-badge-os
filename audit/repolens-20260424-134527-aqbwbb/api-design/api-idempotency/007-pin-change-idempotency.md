---
title: "[LOW] PIN change operations generate new salt on every call (non-idempotent storage writes)"
severity: LOW
domain: api-design/api-idempotency
lens: api-idempotency
labels:
  - audit:api-design/api-idempotency
---

## Summary
The `PinManager::setPW1()`, `PinManager::setPW3()`, and `PinManager::setBadgePin()` functions always generate a new random salt and write to storage, even when setting the same PIN value. This means identical PIN change operations produce different storage results and waste secure element R-Memory write cycles.

**Location:** `components/cdc_core/src/PinManager.cpp:347-371` (setBadgePin), `components/cdc_core/src/PinManager.cpp:452-468` (setPW1), `components/mod_gpg/src/pin_storage.cpp:16-22` (change_pw1, change_pw3)

## Impact
- **Unnecessary storage writes:** Each PIN change writes to R-Memory even if the PIN value is the same
- **Flash wear:** TROPIC01 R-Memory has limited write endurance; unnecessary writes accelerate wear
- **Non-idempotent behavior:** Calling `setPW1("123456")` twice produces different internal state (different salts)
- **No deduplication:** Doesn't check if the new PIN equals the current PIN before regenerating salt

## Evidence
```cpp
// components/cdc_core/src/PinManager.cpp:452-468
bool PinManager::setPW1(const char* newPin) {
    if (!newPin) return false;
    size_t len = strlen(newPin);
    if (len < PW1_MIN || len > PIN_MAX) {
        LOG_E(TAG, "PW1 must be %d-%d digits", PW1_MIN, PIN_MAX);
        return false;
    }

    // Generate new salt (always, even if PIN is same)
    generateSalt(pw1Salt_);
    computeKdfHash(newPin, pw1Salt_, pw1Hash_);
    pw1Retries_ = MAX_RETRIES;

    saveToStorage();  // Always writes to R-Memory
    LOG_I(TAG, "PW1 changed");
    return true;
}
```

Similar pattern in `setBadgePin()`:
```cpp
// components/cdc_core/src/PinManager.cpp:347-371
bool PinManager::setBadgePin(const char* newPin) {
    // ... validation ...

    computeBadgeHash(newPin, badgeHash_);
    badgeRetries_ = MAX_RETRIES;

    uint8_t defaultHash[BADGE_HASH_SIZE];
    computeBadgeHash(DEFAULT_BADGE_PIN, defaultHash);
    badgePinIsSet_ = !compareHash(badgeHash_, defaultHash, BADGE_HASH_SIZE);

    saveToStorage();  // Always writes to R-Memory
    LOG_I(TAG, "Badge PIN changed");
    return true;
}
```

Badge PIN doesn't use salt, but still writes every time.

## Recommended Fix
Add idempotency check to avoid unnecessary storage writes:

1. **Compute hash of new PIN**
2. **Compare with current hash**
3. **Return early** if PIN is the same (idempotent success)
4. **Only generate new salt and write** if PIN actually changed

Example implementation:
```cpp
bool PinManager::setPW1(const char* newPin) {
    if (!newPin) return false;
    size_t len = strlen(newPin);
    if (len < PW1_MIN || len > PIN_MAX) {
        LOG_E(TAG, "PW1 must be %d-%d digits", PW1_MIN, PIN_MAX);
        return false;
    }

    // Compute hash of new PIN with current salt
    uint8_t newHash[KDF_HASH_SIZE];
    computeKdfHash(newPin, pw1Salt_, newHash);

    // Check if PIN is the same (idempotent check)
    if (compareHash(pw1Hash_, newHash, KDF_HASH_SIZE)) {
        LOG_I(TAG, "PW1 unchanged, skipping update");
        pw1Retries_ = MAX_RETRIES;
        return true;  // Idempotent success - no write needed
    }

    // PIN changed - generate new salt and update
    generateSalt(pw1Salt_);
    computeKdfHash(newPin, pw1Salt_, pw1Hash_);
    pw1Retries_ = MAX_RETRIES;

    saveToStorage();
    LOG_I(TAG, "PW1 changed");
    return true;
}
```

For `setBadgePin()`:
```cpp
bool PinManager::setBadgePin(const char* newPin) {
    // ... validation ...

    computeBadgeHash(newPin, badgeHash_);
    badgeRetries_ = MAX_RETRIES;

    // Idempotency check
    uint8_t defaultHash[BADGE_HASH_SIZE];
    computeBadgeHash(DEFAULT_BADGE_PIN, defaultHash);
    bool isSet = !compareHash(badgeHash_, defaultHash, BADGE_HASH_SIZE);

    if (isSet == badgePinIsSet_ && compareHash(badgeHash_, newHash, BADGE_HASH_SIZE)) {
        LOG_I(TAG, "Badge PIN unchanged, skipping update");
        return true;  // Idempotent success
    }

    badgePinIsSet_ = isSet;
    saveToStorage();
    LOG_I(TAG, "Badge PIN changed");
    return true;
}
```

## References
- TROPIC01 R-Memory endurance: ~100K write cycles per slot
- Idempotency pattern: "Same input produces same observable output"
- Similar pattern: FIDO2 `authenticatorMakeCredential` replaces existing credentials (idempotent update)
