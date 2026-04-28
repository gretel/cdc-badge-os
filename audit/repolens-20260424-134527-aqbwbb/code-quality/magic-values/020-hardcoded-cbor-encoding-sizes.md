---
title: "[MEDIUM] Hardcoded CBOR encoding size constants"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
The CBOR encoding helpers use hardcoded size values (24, 25, 26, 27, 31) for CBOR additional info field without named constants. These are CBOR specification values for encoding different integer ranges.

**Location:** `components/mod_fido2/src/cbor_helpers.cpp:343-370`

## Impact
- **Clarity**: Numbers 24, 25, 26, 27, 31 don't immediately convey "CBOR additional info for 8-bit, 16-bit, 32-bit, 64-bit, indefinite".
- **Standards compliance**: These are CBOR spec values that should reference the standard.
- **Maintainability**: CBOR spec reference would help developers understand the encoding.

## Evidence

**Lines 343-370:**
```cpp
uint8_t info = initial & 0x1F;

if (info < 24) {
    *value = info;
} else if (info == 24) {  // 8-bit additional value
    uint8_t b;
    if (!read_byte(r, &b)) return false;
    *value = b;
} else if (info == 25) {  // 16-bit additional value
    uint8_t b[2];
    if (!read_byte(r, &b[0]) || !read_byte(r, &b[1])) return false;
    *value = ((uint16_t)b[0] << 8) | b[1];
} else if (info == 26) {  // 32-bit additional value
    uint8_t b[4];
    for (int i = 0; i < 4; i++) {
        if (!read_byte(r, &b[i])) return false;
    }
    *value = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
             ((uint32_t)b[2] << 8) | b[3];
} else if (info == 27) {  // 64-bit additional value
    uint8_t b[8];
    for (int i = 0; i < 8; i++) {
        if (!read_byte(r, &b[i])) return false;
    }
    *value = ((uint64_t)b[0] << 56) | ((uint64_t)b[1] << 48) |
             ((uint64_t)b[2] << 40) | ((uint64_t)b[3] << 32) |
             ((uint64_t)b[4] << 24) | ((uint64_t)b[5] << 16) |
             ((uint64_t)b[6] << 8) | b[7];
} else if (info == 31) {  // Indefinite length
    // Indefinite length - not supported in CTAP2
    r->error = true;
    LOG_E("CBOR", "Indefinite length not supported");
    return false;
}
```

## Recommended Fix

1. **Define CBOR additional info constants** in `components/mod_fido2/include/mod_fido2/cbor_constants.h`:
    ```cpp
    #pragma once
    #include <cstdint>
    
    /**
     * \brief CBOR encoding constants
     * 
     * Based on:
     * - CBOR RFC 8949 Section 3: Encoding of Additional Information
     */
    namespace cdc::mod_fido2 {
    namespace cbor {
        // Additional info encoding sizes (RFC 8949 Section 3)
        static constexpr uint8_t ADI_8_BIT = 24;     // Next 1 byte
        static constexpr uint8_t ADI_16_BIT = 25;    // Next 2 bytes
        static constexpr uint8_t ADI_32_BIT = 26;    // Next 4 bytes
        static constexpr uint8_t ADI_64_BIT = 27;    // Next 8 bytes
        static constexpr uint8_t ADI_INDEFINITE = 31; // Indefinite length
    }
    }
    ```

2. **Update usage** to use constants:
    ```cpp
    // Before:
    } else if (info == 24) {
        uint8_t b;
        if (!read_byte(r, &b)) return false;
        *value = b;
    } else if (info == 25) {
    
    // After:
    } else if (info == cbor::ADI_8_BIT) {
        uint8_t b;
        if (!read_byte(r, &b)) return false;
        *value = b;
    } else if (info == cbor::ADI_16_BIT) {
    ```

## References
- [CBOR RFC 8949 Section 3](https://datatracker.ietf.org/doc/html/rfc8949#section-3): Encoding of Additional Information
- [CBOR Specification](https://cbor.io/)
