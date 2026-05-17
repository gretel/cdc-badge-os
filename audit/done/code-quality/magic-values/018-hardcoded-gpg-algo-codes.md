---
title: "[MEDIUM] Hardcoded GPG algorithm codes in mod_gpg"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
The GPG module uses hardcoded algorithm codes (19 for P-256, 22 for ED25519) without named constants. These are OpenPGP algorithm identifiers defined in RFC 4880.

**Location:** `components/mod_gpg/src/gpg.cpp:166, 235`

## Impact
- **Clarity**: Magic numbers 19 and 22 don't convey "P-256" and "ED25519" algorithm IDs.
- **Maintainability**: Algorithm IDs must be looked up in the OpenPGP spec for each use.
- **Standards compliance**: These are standard OpenPGP values that should be named.

## Evidence

**Line 166:**
```cpp
uint8_t algo = (curve == CDC_CURVE_ED25519) ? 22 : 19;
```

**Line 235:**
```cpp
uint8_t algo = (curve == CDC_CURVE_ED25519) ? 22 : 
```

## Recommended Fix

1. **Define GPG algorithm codes** in `components/mod_gpg/include/mod_gpg/gpg_constants.h`:
    ```cpp
    #pragma once
    #include <cstdint>
    
    /**
     * \brief GPG/OpenPGP algorithm identifiers
     * 
     * Based on:
     * - OpenPGP RFC 4880 Section 9.1: Algorithm identifiers
     */
    namespace cdc::mod_gpg {
    namespace gpg {
        // Algorithm IDs (RFC 4880 Table 3)
        static constexpr uint8_t ALGO_RSA = 1;
        static constexpr uint8_t ALGO_ELGAMAL = 16;
        static constexpr uint8_t ALGO_ECDSA = 19;
        static constexpr uint8_t ALGO_EDDSA = 22;  // ED25519
        
        // Curve OIDs (RFC 6090)
        static constexpr uint8_t OID_P256[] = {0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07};
        static constexpr uint8_t OID_ED25519[] = {0x09, 0x2B, 0x06, 0x01, 0x04, 0x01, 0xDA, 0x47, 0x0F, 0x01};
    }
    }
    ```

2. **Update usage** to use constants:
    ```cpp
    // Before:
    uint8_t algo = (curve == CDC_CURVE_ED25519) ? 22 : 19;
    
    // After:
    uint8_t algo = (curve == CDC_CURVE_ED25519) ? gpg::ALGO_EDDSA : gpg::ALGO_ECDSA;
    ```

## References
- [OpenPGP RFC 4880 Section 9.1](https://datatracker.ietf.org/doc/html/rfc4880#section-9.1): Algorithm identifiers
- [OpenPGP OIDs RFC 6090](https://datatracker.ietf.org/doc/html/rfc6090)
