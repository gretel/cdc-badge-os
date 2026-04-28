---
title: "[MEDIUM] Hardcoded TOTP hash output sizes in TotpStore"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
The TOTP implementation uses hardcoded hash output sizes (20, 32, 64 bytes) for SHA1, SHA256, and SHA512 respectively without named constants. These values are well-known properties of the hash algorithms but should be defined for clarity.

**Location:** `components/mod_totp/src/TotpStore.cpp:446-454`

## Impact
- **Clarity**: The numbers 20, 32, 64 don't immediately convey "SHA1, SHA256, SHA512 output sizes".
- **Maintainability**: If hash algorithms change, these magic numbers must be found and updated.
- **Self-documentation**: Named constants would make the code self-documenting.

## Evidence

**Lines 440-456:**
```cpp
mbedtls_md_type_t mdType;
size_t expectedLen;

switch (algo) {
    case TotpAlgorithm::SHA256:
        mdType = MBEDTLS_MD_SHA256;
        expectedLen = 32;  // Magic number
        break;
    case TotpAlgorithm::SHA512:
        mdType = MBEDTLS_MD_SHA512;
        expectedLen = 64;  // Magic number
        break;
    default:
        mdType = MBEDTLS_MD_SHA1;
        expectedLen = 20;  // Magic number
        break;
}
```

## Recommended Fix

1. **Define hash output size constants** near the TOTP algorithm enum or in a constants file:
    ```cpp
    /** \brief Hash digest sizes in bytes for common algorithms. */
    static constexpr size_t HASH_SHA1_SIZE = 20;
    static constexpr size_t HASH_SHA256_SIZE = 32;
    static constexpr size_t HASH_SHA512_SIZE = 64;
    ```

2. **Update the switch statement** to use constants:
    ```cpp
    switch (algo) {
        case TotpAlgorithm::SHA256:
            mdType = MBEDTLS_MD_SHA256;
            expectedLen = HASH_SHA256_SIZE;
            break;
        case TotpAlgorithm::SHA512:
            mdType = MBEDTLS_MD_SHA512;
            expectedLen = HASH_SHA512_SIZE;
            break;
        default:
            mdType = MBEDTLS_MD_SHA1;
            expectedLen = HASH_SHA1_SIZE;
            break;
    }
    ```

## References
- [SHA-1](https://en.wikipedia.org/wiki/SHA-1): 160-bit (20 bytes) output
- [SHA-256](https://en.wikipedia.org/wiki/SHA-2): 256-bit (32 bytes) output
- [SHA-512](https://en.wikipedia.org/wiki/SHA-2): 512-bit (64 bytes) output
