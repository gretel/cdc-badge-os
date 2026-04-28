---
title: "[LOW] Inconsistent method naming for PIN operations: getBadgePinHash vs getPW1Hash"
severity: LOW
domain: cdc_core
lens: naming-conventions
labels:
  - "audit:code-quality/naming"
---

## Summary
In `components/cdc_core/include/cdc_core/PinManager.h`, PIN-related methods use inconsistent prefixes. Badge PIN uses full name `getBadgePinHash()`, while OpenPGP PINs use abbreviations `getPW1Hash()`, `getPW3Hash()`.

**Evidence** (lines 47-75):
```cpp
// Badge PIN uses full name
bool getBadgePinHash(uint8_t* hashOut) const;
bool verifyBadgePin(const char* pin);
bool changeBadgePin(const char* currentPin, const char* newPin);
bool setBadgePin(const char* newPin);

// OpenPGP PINs use PW1/PW3 abbreviations
bool getPW1Hash(uint8_t* hashOut) const;
bool verifyPW1(const char* pin);
bool changePW1(const char* currentPin, const char* newPin);
bool setPW1(const char* newPin);

bool getPW3Hash(uint8_t* hashOut) const;
bool verifyPW3(const char* pin);
bool changePW3(const char* currentPin, const char* newPin);
bool setPW3(const char* newPin);
```

## Impact
- **Discoverability**: Developers may search for `getUserPinHash()` or `getAdminPinHash()` instead of `getPW1Hash()`
- **Readability**: PW1/PW3 are OpenPGP-specific terms that may not be clear to all developers
- **Consistency**: Mixed naming makes the API harder to learn

## Evidence
- File: `components/cdc_core/include/cdc_core/PinManager.h`
- Lines: 47-75
- Inconsistent patterns: `getBadgePinHash` vs `getPW1Hash` vs `getPW3Hash`

## Recommended Fix
Choose one consistent naming pattern:

**Option 1: Use descriptive names**
```cpp
bool getBadgeHash(uint8_t* hashOut) const;
bool getUserHash(uint8_t* hashOut) const;   // Instead of PW1
bool getAdminHash(uint8_t* hashOut) const;  // Instead of PW3
bool verifyUserPin(const char* pin);        // Instead of verifyPW1
bool verifyAdminPin(const char* pin);       // Instead of verifyPW3
```

**Option 2: Use consistent abbreviations**
```cpp
bool getBadgeHash(uint8_t* hashOut) const;
bool getPin1Hash(uint8_t* hashOut) const;   // Instead of PW1
bool getPin3Hash(uint8_t* hashOut) const;   // Instead of PW3
bool verifyPin1(const char* pin);           // Instead of verifyPW1
bool verifyPin3(const char* pin);           // Instead of verifyPW3
```

## References
- C++ Core Guidelines: [C.35: Use a consistent naming style](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#C35)
- ESP32 docs: PW1/PW3 are OpenPGP-specific terms (may need documentation)
