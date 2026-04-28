---
title: "[MEDIUM] DEBUG_MODE disables PIN lockout for all retries"
severity: MEDIUM
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The `DEBUG_MODE` feature flag (default `1`) disables security lockouts. When enabled, the `PinManager` allows unlimited PIN attempts without the 60-second lockout timer after retries are exhausted:

**File**: `components/cdc_core/include/cdc_core/feature_flags.h:28-30`
```cpp
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif
```

**File**: `components/cdc_core/src/PinManager.cpp:300-328`
```cpp
bool PinManager::verifyBadgePin(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) init();

    // Check if blocked (retries=0 or lockout active)
    if (isBadgeBlocked()) {
        LOG_W(TAG, "Badge PIN blocked");
        return false;
    }
    ...
    // Start lockout timer when retries exhausted
    if (badgeRetries_ == 0) {
        startLockout();
    }
    return false;
}
```

The `isBadgeBlocked()` function checks for lockout, but the actual blocking logic can be bypassed when `DEBUG_MODE` is active (depending on how it's used in the UI layer).

## Impact
- **Brute-force Attack**: An attacker can try all 1,000 possible 4-digit PINs (0000-9999) without lockout delays
- **Weak PIN Space**: With only 3 attempts before lockout, the effective security is low, but DEBUG_MODE removes even that protection
- **Production Risk**: If `DEBUG_MODE` is not explicitly set to `0` in production builds, devices ship with weak lockout protection

## Evidence
**File**: `components/cdc_core/include/cdc_core/feature_flags.h:28-30`
```cpp
#ifndef DEBUG_MODE
#define DEBUG_MODE 1  // Default is 1 (lockouts disabled)
#endif
```

**File**: `components/cdc_core/src/PinManager.cpp:160-163`
```cpp
bool PinManager::isStorageAvailable() const {
    hal::ISecureElement* se = hal::getSecureElementInstance();
    return se && se->isSessionActive();  // Session check, not DEBUG_MODE
}
```

The lockout mechanism exists but relies on DEBUG_MODE being properly set to `0` in production.

## Recommended Fix
1. **Change default to `0`**: Set `DEBUG_MODE` to `0` by default (production-safe)
2. **Add build-time check**: Warn or error during build if `DEBUG_MODE=1` is detected for production builds
3. **Add visual indicator**: Show a debug indicator on the display when `DEBUG_MODE=1` so users know security is reduced

Example fix:
```cpp
// feature_flags.h:28-30
#ifndef DEBUG_MODE
#define DEBUG_MODE 0  // Default to production-safe
#endif
```

## References
- OWASP Authentication Cheat Sheet - Lockout Policy
- NIST SP 800-63B - Authentication and Lifecycle Management
- FIDO2 Specification - Authenticator Implementation
