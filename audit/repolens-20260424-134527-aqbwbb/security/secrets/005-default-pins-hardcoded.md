---
title: "[MEDIUM] Default PINs hardcoded with weak values (123456, 12345678)"
severity: MEDIUM
domain: secrets
lens: secrets-credential-management
labels:
  - "audit:security/secrets"
---

## Summary
The `PinManager` component uses hardcoded default PINs for Badge/FIDO2 and OpenPGP authentication:
- **Badge/FIDO2 PIN**: `"123456"` (6 digits)
- **OpenPGP PW1 (User PIN)**: `"123456"` (6 digits)
- **OpenPGP PW3 (Admin PIN)**: `"12345678"` (8 digits)

These defaults are defined in `components/cdc_core/include/cdc_core/PinManager.h:53-55` and used in `components/cdc_core/src/PinManager.cpp` (lines 50-61).

## Impact
**Security Risk**: Default PINs are well-known and easily guessable. Users who don't change the defaults have:
- Weak FIDO2 credential protection
- Weak OpenPGP key protection
- Potential unauthorized access to all stored secrets (passwords, TOTP, GPG keys)

The PIN `"123456"` is one of the most common passwords and appears in top 10 password lists globally.

## Evidence

**File: `components/cdc_core/include/cdc_core/PinManager.h`**
```cpp
// Lines 53-55
static constexpr const char* DEFAULT_BADGE_PIN = "123456";
static constexpr const char* DEFAULT_PW1 = "123456";
static constexpr const char* DEFAULT_PW3 = "12345678";
```

**File: `components/cdc_core/src/PinManager.cpp`**
```cpp
// Lines 50-61 - defaults loaded on first init
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

    iterations_ = DEFAULT_ITERATIONS;
    pw1Retries_ = MAX_RETRIES;
    pw3Retries_ = MAX_RETRIES;
    badgePinIsSet_ = false;

    LOG_I(TAG, "Loaded default PINs");
}
```

**File: `components/mod_gpg/src/pin_storage.cpp`**
```cpp
// Lines 49-50 - GPG module also uses defaults
bool ok1 = cdc::core::PinManager::instance().setPW1(cdc::core::PinManager::DEFAULT_PW1);
bool ok3 = cdc::core::PinManager::instance().setPW3(cdc::core::PinManager::DEFAULT_PW3);
```

## Recommended Fix
1. **Force PIN change on first use**: Add a flag to track if defaults are still active and require user to change PINs before full functionality is enabled.

2. **Use stronger defaults**: Change defaults to cryptographically random values:
   ```cpp
   // Generate random 6-8 digit PIN for each device
   static constexpr const char* DEFAULT_BADGE_PIN = "738492";  // Random example
   static constexpr const char* DEFAULT_PW1 = "482916";
   static constexpr const char* DEFAULT_PW3 = "59182734";
   ```

3. **Document clearly**: Add prominent warning in README and UI that users MUST change default PINs.

4. **Add expiration**: Implement PIN expiration after N days of use if not changed from defaults.

## References
- [NIST SP 800-63B: Digital Identity Guidelines](https://pages.nist.gov/800-63-3/sp800-63b.html#sec5112) - Recommends memorized secrets meet complexity requirements
- [OWASP Authentication Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html) - Default credentials best practices
- [Common Weakness Enumeration CWE-798](https://cwe.mitre.org/data/definitions/798.html) - Use of hard-coded credentials
