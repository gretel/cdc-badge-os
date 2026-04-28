---
title: "[HIGH] Default PIN "123456" set without user confirmation or change prompt"
severity: HIGH
domain: compliance
lens: consent-flows
labels:
  - default-credentials
  - user-consent
  - security
---

## Summary

The CDC Badge OS uses a **hardcoded default PIN "123456"** for the Badge PIN and OpenPGP PW1/PW3 PINs. When a user first sets up the device, they are not explicitly prompted to change this default PIN. The PIN is simply set to "123456" (or "12345678" for PW3) and the user must manually navigate to settings to change it.

**Location:** `components/cdc_core/include/cdc_core/PinManager.h:48-50`

```cpp
// Defaults
static constexpr const char* DEFAULT_BADGE_PIN = "123456";
static constexpr const char* DEFAULT_PW1 = "123456";
static constexpr const char* DEFAULT_PW3 = "12345678";
```

**Location:** `components/cdc_core/src/PinManager.cpp:140-170` (loadDefaults function)

```cpp
void PinManager::loadDefaults() {
    // Load default PIN hashes
    computeBadgeHash(DEFAULT_BADGE_PIN, badgeHash_);
    badgeRetries_ = MAX_RETRIES;

    // Compute KDF hashes for OpenPGP PINs
    uint8_t salt[8];
    generateSalt(salt);

    // ... computes hashes for DEFAULT_PW1 and DEFAULT_PW3 ...

    iterations_ = DEFAULT_ITERATIONS;
    badgePinIsSet_ = true;  // Marked as "set" even though it's the default!
}
```

## Impact

1. **Weak security by default:** Any user who doesn't change the default PIN has their device protected by a well-known, predictable PIN. This is especially problematic because:
   - The default is documented in the header file (easy to find)
   - Many users may not realize they should change it
   - The device is a "hardware security key" so security is the primary value proposition

2. **No explicit consent for security settings:** The user hasn't explicitly agreed to use "123456" as their PIN - it's just set as the default.

3. **Badge PIN used for multiple purposes:** The Badge PIN protects:
   - Device unlock
   - Serial command authentication
   - FIDO2 operations (when ClientPIN isn't used)
   
   A weak default PIN compromises all these features.

4. **`isPinSet()` returns true for default:** The function returns `true` even when the default PIN is in use, so code checking "is a PIN set?" cannot distinguish between a user-chosen PIN and the factory default.

## Evidence

**PinManager.h:48-50** - Hardcoded defaults:
```cpp
static constexpr const char* DEFAULT_BADGE_PIN = "123456";
static constexpr const char* DEFAULT_PW1 = "123456";
static constexpr const char* DEFAULT_PW3 = "12345678";
```

**PinManager.cpp:146-168** - Defaults loaded automatically:
```cpp
void PinManager::loadDefaults() {
    // Badge/FIDO2: "123456"
    computeBadgeHash(DEFAULT_BADGE_PIN, badgeHash_);
    badgeRetries_ = MAX_RETRIES;

    // ... generates salts ...

    // PW1 (User): "123456"
    computeKdfHash(DEFAULT_PW1, pw1Salt_, pw1Hash_);

    // PW3 (Admin): "12345678"
    computeKdfHash(DEFAULT_PW3, pw3Salt_, pw3Hash_);

    badgePinIsSet_ = true;  // <-- Returns true even for default!
}
```

**PinManager.cpp:360-375** - `isPinSet()` cannot distinguish default from user-chosen:
```cpp
bool PinManager::isPinSet() const {
    return badgePinIsSet_;  // Just checks the flag, not if it's the default
}
```

No code exists to:
- Prompt user to change default PIN on first boot
- Show a "default PIN in use" warning
- Require PIN change before enabling certain features

## Recommended Fix

Implement a "default PIN change" flow:

1. **Add a flag to track default PIN status:**
```cpp
// In PinManager.h
bool isDefaultPin() const;  // Returns true if current PIN matches default

// In PinManager.cpp
bool PinManager::isDefaultPin() const {
    uint8_t defaultHash[BADGE_HASH_SIZE];
    computeBadgeHash(DEFAULT_BADGE_PIN, defaultHash);
    return memcmp(badgeHash_, defaultHash, BADGE_HASH_SIZE) == 0;
}
```

2. **Add first-boot detection:**
```cpp
// In cdc_core/FeatureFlags.h or new header
bool isFirstBoot();  // Check NVS for first-boot flag
void markFirstBootComplete();
```

3. **Prompt on first boot:**
In the main app or lock screen, check for default PIN on first boot:
```cpp
if (firstBoot && pinManager.isDefaultPin()) {
    // Show modal: "Default PIN detected. Please change it."
    // Navigate to PIN change view
}
```

4. **Add "Change PIN" to main menu** (not buried in Settings):
```cpp
// In AppUi.cpp or similar
items[0] = {"Change PIN", ...};  // Prominent location
```

5. **Optional: Show warning toast** when default PIN is in use:
```cpp
void PinManager::checkDefaultPin() {
    if (isDefaultPin()) {
        ui::showToastWarning("Default PIN in use - change recommended!", 3000);
    }
}
```

6. **Consider requiring PIN change** before certain operations (e.g., enabling BLE, storing first credential).

## References

- OWASP Authentication Cheat Sheet: "Ensure default credentials are changed"
- NIST SP 800-63B: "Implementers SHOULD require the subscriber to change the initial secret"
- FIDO2 spec: Recommends user-chosen PINs for ClientPIN protocol

</content>