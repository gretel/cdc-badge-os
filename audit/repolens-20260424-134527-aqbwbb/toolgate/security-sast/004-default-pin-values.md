---
title: "[MEDIUM] Default PIN values documented in header files"
severity: MEDIUM
domain: security
lens: sast
labels:
  - "password"
  - "defaults"
  - "PIN"
---

## Summary
Default PIN values are defined as compile-time constants in `components/cdc_core/include/cdc_core/PinManager.h`, making them visible to anyone who reads the header file. The defaults are "123456" for Badge/FIDO2 and OpenPGP PW1, and "12345678" for PW3.

## Impact
- **Weak Defaults**: "123456" and "12345678" are predictable and commonly used
- **Information Disclosure**: Header files are public, so default PINs are easily discoverable
- **User Behavior**: Users may not change defaults, leaving devices with well-known PINs

## Evidence
File: `components/cdc_core/include/cdc_core/PinManager.h:54-56`
```cpp
// Defaults:
// - Badge/FIDO2: "123456"
// - OpenPGP PW1 (User): "123456" (min 6 digits)
// - OpenPGP PW3 (Admin): "12345678" (min 8 digits)

static constexpr const char* DEFAULT_BADGE_PIN = "123456";
static constexpr const char* DEFAULT_PW1 = "123456";
static constexpr const char* DEFAULT_PW3 = "12345678";
```

## Recommended Fix

**Option 1: Require user to set PIN on first use**
- Remove default PINs entirely
- Force user to configure PIN during initial setup
- Show "PIN not set" status until configured

**Option 2: Use cryptographically random defaults**
- Generate random PIN at first boot
- Store in NVS
- Show random PIN to user on first boot (via display or serial)

**Option 3: Use stronger, less predictable defaults**
- If defaults must exist, use non-sequential values
- Document that users MUST change them
- Add warning on first boot

**Example implementation (Option 1):**
```cpp
// Remove DEFAULT_BADGE_PIN, DEFAULT_PW1, DEFAULT_PW3
// In init():
bool PinManager::init() {
    if (!loadFromStorage()) {
        // No PIN set, require user to configure
        LOG_I(TAG, "No PIN configured, requiring setup");
        pinLoaded_ = false;
        badgePinIsSet_ = false;
        return true;  // Success, but PIN must be set
    }
    ...
}
```

## References
- CWE-798: Use of hardcoded credentials
- CWE-521: Weak password requirements
- NIST SP 800-63B: Digital Identity Guidelines
