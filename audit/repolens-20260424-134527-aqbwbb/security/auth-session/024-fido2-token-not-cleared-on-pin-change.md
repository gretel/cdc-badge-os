---
title: "[MEDIUM] FIDO2 session state not invalidated when badge PIN is changed"
severity: MEDIUM
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
When the badge PIN is changed via the UI or serial commands, both the FIDO2 `pin_token_valid` state AND the `pin_verified` flag are **not cleared**. This means:
1. An existing `pin_token` could remain valid even after the PIN has been changed
2. The `pin_verified` flag allows skipping PIN verification even after the PIN is changed

This allows an attacker who captured a token or verified session to continue using it even after the user changes their PIN.

**File**: `components/cdc_core/src/PinManager.cpp:347-371`
```cpp
bool PinManager::setBadgePin(const char* newPin) {
    if (!newPin) return false;
    // ... validation ...
    
    computeBadgeHash(newPin, badgeHash_);
    badgeRetries_ = MAX_RETRIES;
    // ...
    saveToStorage();
    LOG_I(TAG, "Badge PIN changed");
    return true;  // No FIDO2 token invalidation!
}
```

**File**: `components/cdc_core/src/PinManager.cpp:452-493`
```cpp
bool PinManager::setPW1(const char* newPin) {
    if (!newPin) return false;
    // ... validation ...
    
    computePW1Hash(newPin, pw1Hash_);
    pw1Retries_ = MAX_RETRIES;
    saveToStorage();
    LOG_I(TAG, "PW1 updated");
    return true;  // No FIDO2 token invalidation!
}
```

**File**: `components/mod_fido2/src/ctap2.cpp:103-116`
```cpp
static struct {
    // ECDH key pair
    mbedtls_ecp_keypair ecdh_key;
    
    // Shared secret (computed on getPinToken)
    uint8_t shared_secret[32];
    
    // PIN token (regenerated on each getPinToken)
    uint8_t pin_token[PIN_TOKEN_SIZE];
    bool pin_token_valid;  // Token state - NOT cleared on PIN change!
    
    // Permission tracking (CTAP 2.1)
    uint8_t token_permissions;
    char token_rp_id[64];
    
    // Retry counters
    uint8_t pin_retries;
    uint8_t uv_retries;
} g_client_pin = {};
```

The `pin_token_valid` flag is only cleared in specific scenarios:
- When `pin_token_valid` is checked and found stale
- On system reset (RAM cleared)
- When a new `pin_token` is generated (overwrites old one)

But **NOT** when the badge PIN is changed.

## Impact
- **Token Reuse**: An attacker who captured a `pin_token` can continue using it even after the user changes their PIN
- **Session Persistence**: The `pin_verified` flag allows skipping PIN verification even after the PIN is changed
- **Physical Access Attack**: If an attacker has a valid token or session and physical access, they can use it even if the user changes the PIN to lock them out
- **FIDO2 Compliance**: CTAP 2.1 expects tokens and session state to be invalidated when the PIN changes

## Evidence
**File**: `components/cdc_core/src/PinManager.cpp:347-371`
```cpp
bool PinManager::setBadgePin(const char* newPin) {
    // ... changes PIN ...
    saveToStorage();
    LOG_I(TAG, "Badge PIN changed");
    return true;  // No call to invalidate FIDO2 token or pin_verified!
}
```

**File**: `components/cdc_core/src/PinManager.cpp:452-493`
```cpp
bool PinManager::setPW1(const char* newPin) {
    // ... changes PW1 ...
    saveToStorage();
    LOG_I(TAG, "PW1 updated");
    return true;  // No call to invalidate FIDO2 token or pin_verified!
}
```

**Search for token invalidation**:
```
grep -n "pin_token_valid.*false\|invalidate.*token\|clear.*token" components/cdc_core/src/PinManager.cpp
# No results found!

grep -n "fido2_set_pin_verified.*false" components/cdc_core/src/PinManager.cpp
# No results found!
```

**File**: `components/mod_fido2/src/ctap2.cpp:114`
```cpp
bool pin_token_valid;  // Cleared on getPinToken, but NOT on PIN change
```

**File**: `components/mod_fido2/src/fido2.cpp:32`
```cpp
bool pin_verified;  // Set on PIN verification, but NOT cleared on PIN change
```

Both the token state and `pin_verified` flag are only cleared when explicitly called, not when the underlying PIN changes.

## Recommended Fix
Add a callback mechanism to invalidate FIDO2 tokens and clear `pin_verified` when the PIN is changed:

1. **Add invalidation function**: Create a function in the FIDO2 module that clears both token and verified state
2. **Call on PIN change**: Call the function from `setBadgePin()`, `setPW1()`, and `setPW3()`

Example implementation:
```cpp
// In mod_fido2/pin_storage.h
void fido2_invalidate_session(void);

// In mod_fido2/src/ctap2.cpp
void fido2_invalidate_session(void) {
    g_client_pin.pin_token_valid = false;
    g_client_pin.token_permissions = 0;
    LOG_I("PIN", "FIDO2 token invalidated due to PIN change");
}

// In mod_fido2/src/fido2.cpp
void fido2_invalidate_session(void) {
    g_fido2.pin_verified = false;
    LOG_I("FIDO2", "pin_verified flag cleared due to PIN change");
}

// In components/cdc_core/src/PinManager.cpp
#include "mod_fido2/pin_storage.h"  // Add include

bool PinManager::setBadgePin(const char* newPin) {
    if (!newPin) return false;
    // ... validation ...
    
    computeBadgeHash(newPin, badgeHash_);
    badgeRetries_ = MAX_RETRIES;
    // ...
    saveToStorage();
    LOG_I(TAG, "Badge PIN changed");
    
    // Invalidate FIDO2 session
    fido2_invalidate_session();
    
    return true;
}

bool PinManager::setPW1(const char* newPin) {
    if (!newPin) return false;
    // ... validation ...
    
    computePW1Hash(newPin, pw1Hash_);
    pw1Retries_ = MAX_RETRIES;
    saveToStorage();
    LOG_I(TAG, "PW1 updated");
    
    // Invalidate FIDO2 session
    fido2_invalidate_session();
    
    return true;
}
```

## References
- CTAP 2.1 Specification - pinUvAuthToken Lifecycle
- NIST SP 800-63B - Session Invalidation
- OWASP Session Management Cheat Sheet - Session Expiration

</content>