---
title: "[MEDIUM] Hardcoded ECDSA/Ed25519 signature sizes in FIDO2 storage"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
The FIDO2 storage implementation uses hardcoded signature sizes (32, 64 bytes) for ECDSA P-256 and Ed25519 signatures without named constants. These are standard cryptographic signature sizes but should be defined for clarity.

**Location:** `components/mod_fido2/src/fido2_storage.cpp`

## Impact
- **Clarity**: Numbers 32 and 64 don't immediately convey "ECDSA P-256 R/S components" and "Ed25519 signature size".
- **Maintainability**: If signature formats change, these values must be found across the codebase.
- **Standards compliance**: These are well-defined cryptographic constants that should be named.

## Evidence

**Line 312:**
```cpp
size_t raw_len = 64;  // Raw ECDSA signature (R || S, 32 bytes each)
```

**Lines 999, 1008:**
```cpp
*sig_len = 64;  // Ed25519 signature is always 64 bytes
*sig_len = 64;  // Raw signature is always 64 bytes for P-256
```

**Lines 275-293 (ECDSA R/S calculation):**
```cpp
uint8_t r_len = 32 - r_skip + r_pad;
uint8_t s_len = 32 - s_skip + s_pad;
p += 32 - r_skip;
p += 32 - s_skip;
```

## Recommended Fix

1. **Define cryptographic size constants** in `components/mod_fido2/include/mod_fido2/crypto_constants.h`:
    ```cpp
    #pragma once
    #include <cstdint>
    
    /**
     * \brief Cryptographic signature and component sizes
     * 
     * Based on:
     * - NIST P-256 (secp256r1): 32-byte R and S components
     * - Ed25519: 64-byte signature
     */
    namespace cdc::mod_fido2 {
    namespace crypto {
        static constexpr size_t ECDSA_P256_COMPONENT_SIZE = 32;  // R or S
        static constexpr size_t ECDSA_P256_SIGNATURE_SIZE = 64;  // R || S
        static constexpr size_t ED25519_SIGNATURE_SIZE = 64;
        static constexpr size_t SHA256_DIGEST_SIZE = 32;
        static constexpr size_t SHA1_DIGEST_SIZE = 20;
    }
    }
    ```

2. **Update usage** to use constants:
    ```cpp
    // Before:
    size_t raw_len = 64;
    
    // After:
    size_t raw_len = crypto::ECDSA_P256_SIGNATURE_SIZE;
    
    // Before:
    uint8_t r_len = 32 - r_skip + r_pad;
    
    // After:
    uint8_t r_len = crypto::ECDSA_P256_COMPONENT_SIZE - r_skip + r_pad;
    
    // Before:
    *sig_len = 64;  // Ed25519 signature is always 64 bytes
    
    // After:
    *sig_len = crypto::ED25519_SIGNATURE_SIZE;
    ```

## References
- [NIST P-256](https://en.wikipedia.org/wiki/Elliptic_Curve_Digital_Signature_Algorithm): 256-bit curve, 32-byte components
- [Ed25519](https://en.wikipedia.org/wiki/EdDSA): 64-byte signatures
- [FIDO2 CTAP2](https://fidoalliance.org/specs/fido-v2.1-rd-20191217/fido-client-to-authenticator-protocol-v2.1-rd-20191217.html)
