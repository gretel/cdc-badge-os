---
title: "[LOW] PinManager::compareHash uses non-constant-time comparison"
severity: LOW
domain: security
lens: session-nuclei
labels:
  - audit:toolgate/session-nuclei
---

## Summary

In `components/cdc_core/src/PinManager.cpp:282-288`, the `compareHash` function uses a simple XOR loop that may not be constant-time:

```cpp
bool PinManager::compareHash(const uint8_t* h1, const uint8_t* h2, size_t len) const {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= h1[i] ^ h2[i];
    }
    return diff == 0;
}
```

While the algorithm itself is correct (accumulating differences with OR), modern compilers may optimize this in ways that leak timing information.

## Impact

- **Timing attack risk**: In theory, an attacker could measure response times to determine how many bytes matched before finding a difference.
- **PIN brute-force**: While the 3-attempt lockout mitigates this, precise timing attacks could still help narrow down the correct PIN.
- **FIDO2 security**: For FIDO2 ClientPIN, timing attacks on the hash comparison could theoretically leak information about the PIN hash.

## Evidence

**File**: `components/cdc_core/src/PinManager.cpp` (lines 282-288)

```cpp
bool PinManager::compareHash(const uint8_t* h1, const uint8_t* h2, size_t len) const {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= h1[i] ^ h2[i];
    }
    return diff == 0;
}
```

The function is called from:
- `verifyBadgePin` (line 315)
- `setBadgePin` (lines 362, 365)
- `getBadgePinHash` verification

## Recommended Fix

Use mbedtls's constant-time comparison function or implement a compiler-resistant version:

**Option 1: Use mbedtls constant-time function**
```cpp
#include "mbedtls/common.h"

bool PinManager::compareHash(const uint8_t* h1, const uint8_t* h2, size_t len) const {
    return mbedtls_mpi_safe_const_compare(h1, h2, len) == 0;
}
```

**Option 2: Add volatile to prevent compiler optimization**
```cpp
bool PinManager::compareHash(const uint8_t* h1, const uint8_t* h2, size_t len) const {
    volatile uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= h1[i] ^ h2[i];
    }
    return diff == 0;
}
```

**Option 3: Use mbedtls.timing.h**
Check if `mbedtls_timing.h` provides a suitable constant-time comparison function.

## References

- mbedtls documentation: `mbedtls_mpi_safe_const_compare`
- CWE-208: Observable timing discrepancy
- FIDO2 Security Considerations: Timing attacks
