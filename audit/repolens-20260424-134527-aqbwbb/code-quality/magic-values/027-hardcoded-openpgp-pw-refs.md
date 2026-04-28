---
title: "[MEDIUM] Hardcoded OpenPGP password reference codes (0x81, 0x82, 0x83)"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
The OpenPGP implementation uses hardcoded values `0x81`, `0x82`, `0x83` for password reference codes throughout `components/mod_gpg/src/openpgp/openpgp.cpp`. These are OpenPGP card specification constants but appear without named definitions.

**Locations:** Lines 1052, 1057, 1080, 1091, 1134, 1173

## Impact
- **Clarity**: Values `0x81`, `0x82`, `0x83` don't immediately convey "PW1 (User PIN)", "PW3 (Admin PIN)".
- **Maintainability**: If OpenPGP card spec changes, all occurrences must be found.
- **Documentation**: No explanation of what each code represents.

## Evidence

**Lines 1052-1065 (PIN verification query):**
```cpp
if (pw_ref == 0x81 || pw_ref == 0x82) {
    if (pin_storage_openpgp_pw1_blocked()) {
        return apdu_sw(resp, SW_AUTH_METHOD_BLOCKED);
    }
    retries = pin_storage_openpgp_pw1_retries();
} else if (pw_ref == 0x83) {
    if (pin_storage_openpgp_pw3_blocked()) {
        return apdu_sw(resp, SW_AUTH_METHOD_BLOCKED);
    }
    retries = pin_storage_openpgp_pw3_retries();
}
```

**Lines 1080-1173 (PIN verification):**
```cpp
if (pw_ref == 0x81 || pw_ref == 0x82) {
    // PW1 (User PIN) verification
    verified = pin_storage_openpgp_verify_pw1(pin_str);
    ...
} else if (pw_ref == 0x83) {
    // PW3 (Admin PIN) verification
    verified = pin_storage_openpgp_verify_pw3(pin_str);
    ...
}
```

**Lines 1134, 1173 (PIN change):**
```cpp
if (pw_ref == 0x81) {
    // Change PW1
} else if (pw_ref == 0x83) {
    // Change PW3
}
```

The values are used for:
- `0x81` / `0x82`: PW1 (User PIN) - Code 1
- `0x83`: PW3 (Admin PIN) - Code 3

## Recommended Fix

1. **Define named constants** for OpenPGP password references:
```cpp
/**
 * \brief OpenPGP Smart Card password reference codes
 * 
 * Based on OpenPGP Card Specification 3.4.1
 * Reference: https://www.g10code.com/p-card.html
 */
namespace openpgp {
    static constexpr uint8_t PW1_CODE_1 = 0x81;     // PW1, Code 1 (User PIN)
    static constexpr uint8_t PW1_CODE_2 = 0x82;     // PW1, Code 2 (alternative)
    static constexpr uint8_t PW3_CODE = 0x83;       // PW3, Code 3 (Admin PIN)
}
```

2. **Update usage** to use constants:
```cpp
// Before:
if (pw_ref == 0x81 || pw_ref == 0x82) {
    // PW1 (User PIN) verification
    ...
} else if (pw_ref == 0x83) {
    // PW3 (Admin PIN) verification
    ...
}

// After:
if (pw_ref == openpgp::PW1_CODE_1 || pw_ref == openpgp::PW1_CODE_2) {
    // PW1 (User PIN) verification
    ...
} else if (pw_ref == openpgp::PW3_CODE) {
    // PW3 (Admin PIN) verification
    ...
}
```

3. **Add context** in documentation:
```cpp
/**
 * \brief OpenPGP password reference codes
 * 
 * OpenPGP Card Specification defines:
 * - PW1, Code 1 (0x81): User PIN for signature/authentication
 * - PW1, Code 2 (0x82): Alternative user PIN (optional)
 * - PW3, Code 3 (0x83): Admin PIN for card administration
 * 
 * Reference: OpenPGP Card Specification 3.4.1, Section 6
 */
namespace openpgp {
    static constexpr uint8_t PW1_CODE_1 = 0x81;
    static constexpr uint8_t PW1_CODE_2 = 0x82;
    static constexpr uint8_t PW3_CODE = 0x83;
}
```

## References
- [OpenPGP Card Specification 3.4.1](https://www.g10code.com/p-card.html): Section 6
- [OpenPGP Smart Card Application](https://github.com/ANSSI-FR/openpgp-card)
