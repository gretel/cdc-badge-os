---
title: "[MEDIUM] Default PIN Values Well-Known and Documented"
severity: MEDIUM
domain: cdc-badge-os
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

The default PIN values (`123456` for Badge/FIDO2 and OpenPGP PW1, `12345678` for OpenPGP PW3) are well-known and documented in the header file. Users who don't change the default PINs are vulnerable to brute-force attacks using these common defaults.

**Location:** `components/cdc_core/include/cdc_core/PinManager.h:26-28, 53-55`

```cpp
/**
 * Defaults:
 * - Badge/FIDO2: "123456"
 * - OpenPGP PW1 (User): "123456" (min 6 digits)
 * - OpenPGP PW3 (Admin): "12345678" (min 8 digits)
 */
static constexpr const char* DEFAULT_BADGE_PIN = "123456";
static constexpr const char* DEFAULT_PW1 = "123456";
static constexpr const char* DEFAULT_PW3 = "12345678";
```

## Impact

**Security Impact:**
- **Well-known default PINs are easily guessable**
- First-time users who don't change the PIN are vulnerable
- The PINs are documented in the source code header
- An attacker with physical access can try these common defaults

**Attack scenarios:**
1. **First-time setup:** User receives device, forgets to change PIN → attacker tries `123456`
2. **Inventory/warehouse:** Devices pre-loaded with default PINs → bulk compromise
3. **Lost device:** User assumes PIN was changed → attacker tries defaults
4. **Documentation leak:** Source code with defaults becomes public

**Current protection:**
- 3-attempt limit before lockout (60s wait)
- But attacker can try all 3 defaults quickly
- Power-cycle bypasses lockout (see separate issue)

## Evidence

1. **File:** `components/cdc_core/include/cdc_core/PinManager.h:26-28`
   - Documents default PINs in header comment

2. **File:** `components/cdc_core/include/cdc_core/PinManager.h:53-55`
   - Defines default PINs as public constants

3. **File:** `components/cdc_core/src/PinManager.cpp:48-65` (`loadDefaults()`)
   ```cpp
   void PinManager::loadDefaults() {
       // Badge/FIDO2 hash
       computeBadgeHash(DEFAULT_BADGE_PIN, badgeHash_);
       badgeRetries_ = MAX_RETRIES;
       ...
   }
   ```

4. **File:** `components/cdc_core/src/PinManager.cpp:146-148`
   ```cpp
   // Check if badge PIN differs from default
   uint8_t defaultHash[BADGE_HASH_SIZE];
   computeBadgeHash(DEFAULT_BADGE_PIN, defaultHash);
   badgePinIsSet_ = !compareHash(badgeHash_, defaultHash, BADGE_HASH_SIZE);
   ```
   - Device tracks if default PIN was changed

## Recommended Fix

**Option 1: Require PIN change on first use**

Add a flag to track if defaults have been changed:
```cpp
// In PinManager.h
bool defaultsChanged_ = false;

// In loadDefaults()
defaultsChanged_ = false;

// In setBadgePin(), setPW1(), setPW3()
defaultsChanged_ = true;
saveToStorage();

// Add getter
bool areDefaultsChanged() const { return defaultsChanged_; }
```

Then check in UI/serial commands:
```cpp
if (!pm.areDefaultsChanged()) {
    Console::printf("WARNING: Default PIN still in use. Change it!\r\n");
}
```

**Option 2: Use random default PINs**

Generate a random default PIN on first boot:
```cpp
void PinManager::loadDefaults() {
    // Generate random 6-digit PIN
    uint8_t randomBytes[4];
    esp_fill_random(randomBytes, sizeof(randomBytes));
    char randomPin[7];
    snprintf(randomPin, sizeof(randomPin), "%06u", *(uint32_t*)randomBytes % 1000000);
    
    computeBadgeHash(randomPin, badgeHash_);
    ...
}
```

**Option 3: Force PIN change on first authentication**

```cpp
bool PinManager::verifyBadgePin(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) init();
    
    // Check if default PIN is still in use
    uint8_t defaultHash[BADGE_HASH_SIZE];
    computeBadgeHash(DEFAULT_BADGE_PIN, defaultHash);
    bool isDefault = compareHash(badgeHash_, defaultHash, BADGE_HASH_SIZE);
    
    if (isDefault && !defaultsChanged_) {
        LOG_W(TAG, "Default PIN detected - force change");
        // Redirect to PIN change flow
        return true;  // Accept but mark for change
    }
    ...
}
```

**Option 4: Increase minimum PIN length**

Current minimum is 4 digits for badge PIN - increase to 6:
```cpp
static constexpr uint8_t BADGE_PIN_MIN = 6;  // Changed from 4
```

**Option 5: Add documentation warning**

```cpp
/**
 * Defaults:
 * - Badge/FIDO2: "123456" (CHANGE THIS ON FIRST USE!)
 * - OpenPGP PW1 (User): "123456" (CHANGE THIS ON FIRST USE!)
 * - OpenPGP PW3 (Admin): "12345678" (CHANGE THIS ON FIRST USE!)
 */
```

## References

- [OWASP Default Credentials Cheat Sheet](https://owasp.org/www-project-default-credentials/)
- NIST SP 800-63B: "Memorized secrets SHALL be at least 8 characters"
- Common security practice: Default credentials should be unique per device
