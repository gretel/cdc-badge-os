---
title: "[MEDIUM] Secure element session not checked before PIN verification"
severity: MEDIUM
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
PIN verification methods call `init()` which loads PINs from storage, but the secure element session check is not robust. If the session is lost temporarily, the PIN verification might fail silently or use stale data.

**File**: `components/cdc_core/src/PinManager.cpp:300-308`
```cpp
bool PinManager::verifyBadgePin(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) init();

    // Check if blocked (retries=0 or lockout active)
    if (isBadgeBlocked()) {
        LOG_W(TAG, "Badge PIN blocked");
        return false;
    }
    // ...
}
```

**File**: `components/cdc_core/src/PinManager.cpp:32-42`
```cpp
bool PinManager::init() {
    if (pinLoaded_) return true;

    if (!loadFromStorage()) {
        LOG_I(TAG, "No PINs stored, using defaults");
        loadDefaults();
    }

    pinLoaded_ = true;
    return true;
}
```

**File**: `components/cdc_core/src/PinManager.cpp:96-107`
```cpp
bool PinManager::loadFromStorage() {
    hal::ISecureElement* se = hal::getSecureElementInstance();
    if (!se || !se->isSessionActive()) {
        LOG_W(TAG, "SE session not active");
        return false;
    }
    // ...
}
```

If `loadFromStorage()` fails, `loadDefaults()` is called, which sets default PINs. This means if the secure element session is lost, the system falls back to default PINs.

## Impact
- **Default PIN Fallback**: If the secure element session is lost, the system uses default PINs ("123456")
- **Stale Data**: PIN verification might use outdated data if the session was re-established
- **Silent Failure**: Verification might succeed with defaults instead of the actual stored PIN

## Evidence
**File**: `components/cdc_core/src/PinManager.cpp:32-42`
```cpp
bool PinManager::init() {
    if (pinLoaded_) return true;

    if (!loadFromStorage()) {
        LOG_I(TAG, "No PINs stored, using defaults");
        loadDefaults();  // Uses default PINs if storage fails
    }
    // ...
}
```

**File**: `components/cdc_core/src/PinManager.cpp:48-66`
```cpp
void PinManager::loadDefaults() {
    computeBadgeHash(DEFAULT_BADGE_PIN, badgeHash_);
    badgeRetries_ = MAX_RETRIES;
    // ...
    badgePinIsSet_ = false;
}
```

The defaults are "123456" for badge PIN.

**File**: `components/mod_fido2/src/pin_storage.cpp:15-20**
```cpp
bool pin_storage_fido2_available(void) {
    auto& pm = cdc::core::PinManager::instance();
    pm.init();
    uint8_t hash[cdc::core::PinManager::BADGE_HASH_SIZE] = {};
    return pm.getBadgePinHash(hash);
}
```

FIDO2 PIN verification also relies on `init()`.

## Recommended Fix
Add explicit session check and return error if session is not available:

```cpp
bool PinManager::verifyBadgePin(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) {
        if (!init()) {
            return false;  // Session not available
        }
    }

    // Check if blocked
    if (isBadgeBlocked()) {
        LOG_W(TAG, "Badge PIN blocked");
        return false;
    }

    // Verify PIN
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

// Update init() to return bool
bool PinManager::init() {
    if (pinLoaded_) return true;

    if (!loadFromStorage()) {
        // Check if it's first time or session lost
        hal::ISecureElement* se = hal::getSecureElementInstance();
        if (!se || !se->isSessionActive()) {
            LOG_W(TAG, "Session not available, defaults not loaded");
            return false;  // Don't use defaults if session lost
        }
        LOG_I(TAG, "No PINs stored, using defaults");
        loadDefaults();
    }

    pinLoaded_ = true;
    return true;
}
```

Also, clear `pinLoaded_` when session is lost:

```cpp
void PinManager::onSessionLost() {
    pinLoaded_ = false;
    badgeRetries_ = MAX_RETRIES;  // Reset to force re-check
    lockoutActive_ = false;
}
```

## References
- NIST SP 800-63B - Authentication and Secure Storage
- OWASP Authentication Cheat Sheet - Session Management
- CWE-388: Error Handling

</content>