---
title: "[HIGH] Hard-coded default PIN values exposed in production"
severity: HIGH
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The `PinManager` component uses hard-coded default PIN values that are well-known and easily guessable:
- **Badge/FIDO2 PIN**: `"123456"` (line 53, `PinManager.h`)
- **OpenPGP PW1 (User)**: `"123456"` (line 54, `PinManager.h`)  
- **OpenPGP PW3 (Admin)**: `"12345678"` (line 55, `PinManager.h`)

These defaults are applied when no stored PIN is found (e.g., first boot, factory reset):
```cpp
// PinManager.cpp:48-65
void PinManager::loadDefaults() {
    // Badge/FIDO2 hash
    computeBadgeHash(DEFAULT_BADGE_PIN, badgeHash_);
    badgeRetries_ = MAX_RETRIES;

    // Generate random salts
    generateSalt(pw1Salt_);
    generateSalt(pw3Salt_);

    // Compute KDF hashes with salts
    computeKdfHash(DEFAULT_PW1, pw1Salt_, pw1Hash_);
    computeKdfHash(DEFAULT_PW3, pw3Salt_, pw3Hash_);
    ...
}
```

## Impact
- **Unauthorized Access**: Any user with physical access can use the default PINs to unlock the device
- **Weak Security Baseline**: Devices shipped with these defaults provide minimal security until changed
- **Admin PIN Exposure**: PW3 (admin PIN) defaults to `"12345678"` - a common 8-digit pattern
- **FIDO2 Compromise**: Default badge PIN affects FIDO2/WebAuthn authentication

## Evidence
**File**: `components/cdc_core/include/cdc_core/PinManager.h:53-55`
```cpp
static constexpr const char* DEFAULT_BADGE_PIN = "123456";
static constexpr const char* DEFAULT_PW1 = "123456";
static constexpr const char* DEFAULT_PW3 = "12345678";
```

**File**: `components/cdc_core/src/PinManager.cpp:48-65`
Default PINs are loaded when storage is empty, making them the initial authentication state.

## Recommended Fix
1. **Force PIN setup on first use**: Require the user to set a custom PIN before any functionality is available (no defaults)
2. **Use cryptographically random defaults**: If defaults are absolutely necessary, generate random PINs at build time and store them in a secure location
3. **Add initialization flag**: Track whether PINs have been initialized and require setup before enabling features

Example implementation:
```cpp
// Add to PinManager.h
bool isInitialized() const { return badgePinIsSet_; }
void setInitialized();  // Called after user sets first PIN

// In loadDefaults(), also set a flag that requires setup
// In verifyBadgePin(), check if initialized and redirect to setup if not
```

## References
- NIST SP 800-63B: Digital Identity Guidelines - Authentication and Lifecycle Management
- FIDO2 Specification - Authenticator Configuration
- OWASP Authentication Cheat Sheet
