---
title: "[MEDIUM] Abbreviation inconsistency: PIN vs PW1/PW3 in PinManager"
severity: MEDIUM
domain: naming-conventions
lens: code-quality
labels:
  - "audit:code-quality/naming"
---

## Summary
The `PinManager` class uses mixed terminology for PIN values. The badge PIN is consistently called `badgePin` but OpenPGP PINs use the abbreviations `PW1` and `PW3` without clear documentation of what these abbreviations mean.

**Evidence:**

**components/cdc_core/include/cdc_core/PinManager.h** (lines 56-99):
```cpp
// === Badge/FIDO2 PIN ===
bool verifyBadgePin(const char* pin);
bool changeBadgePin(const char* currentPin, const char* newPin);
bool setBadgePin(const char* newPin);
bool getBadgePinHash(uint8_t* hashOut) const;

// === OpenPGP PW1 (User PIN) ===
bool verifyPW1(const char* pin);
bool changePW1(const char* currentPin, const char* newPin);
bool setPW1(const char* newPin);
bool getPW1Hash(uint8_t* hashOut) const;
bool getPW1Salt(uint8_t* saltOut) const;
uint8_t getPW1Retries() const { return pw1Retries_; }

// === OpenPGP PW3 (Admin PIN) ===
bool verifyPW3(const char* pin);
bool changePW3(const char* currentPin, const char* newPin);
bool setPW3(const char* newPin);
bool getPW3Hash(uint8_t* hashOut) const;
```

Member variables (lines 113-127):
```cpp
uint8_t badgeRetries_ = MAX_RETRIES;
uint8_t pw1Salt_[SALT_SIZE] = {};
uint8_t pw3Salt_[SALT_SIZE] = {};
uint8_t pw1Hash_[KDF_HASH_SIZE] = {};
uint8_t pw3Hash_[KDF_HASH_SIZE] = {};
uint8_t pw1Retries_ = MAX_RETRIES;
uint8_t pw3Retries_ = MAX_RETRIES;
```

## Impact
- **Discoverability**: Developers unfamiliar with OpenPGP terminology don't know what `PW1` and `PW3` mean
- **Inconsistency**: Badge PIN uses full word `badgePin` while OpenPGP PINs use abbreviations
- **Learning curve**: New developers must look up OpenPGP spec to understand the naming

## Evidence
The class mixes:
- Full descriptive names: `badgePin`, `badgeRetries_`
- Abbreviated names: `pw1Retries_`, `pw3Salt_`, `pw1Hash_`

While comments explain "PW1 (User PIN)" and "PW3 (Admin PIN)", the actual method and variable names use the abbreviated form only.

## Recommended Fix
Use consistent, descriptive naming for all PIN types:

**Option A: Explicit naming**
```cpp
// === OpenPGP User PIN (PW1) ===
bool verifyUserPin(const char* pin);
bool changeUserPin(const char* currentPin, const char* newPin);
bool setUserPin(const char* newPin);
bool getUserPinHash(uint8_t* hashOut) const;
uint8_t getUserPinRetries() const { return userPinRetries_; }

// === OpenPGP Admin PIN (PW3) ===
bool verifyAdminPin(const char* pin);
bool changeAdminPin(const char* currentPin, const char* newPin);
bool setAdminPin(const char* newPin);
bool getAdminPinHash(uint8_t* hashOut) const;
uint8_t getAdminPinRetries() const { return adminPinRetries_; }
```

**Option B: Keep PW1/PW3 but make it consistent**
```cpp
// If you want to keep the OpenPGP terminology, use it everywhere:
bool verifyBadgePin();      // Keep badge as full word (unique to this device)
bool verifyPw1();           // Use consistent lowercase abbreviation
bool verifyPw3();
```

**Steps:**
1. Decide on naming convention (recommended: Option A with explicit names)
2. Rename all methods and member variables in `PinManager.h` and `PinManager.cpp`
3. Update all call sites throughout the codebase
4. Update comments and documentation

## References
- [OpenPGP Card Specification](https://g10code.com/docs/openpgp-card.html) - Explains PW1 and PW3 terminology
- [C++ Core Guidelines - Naming](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-naming)
