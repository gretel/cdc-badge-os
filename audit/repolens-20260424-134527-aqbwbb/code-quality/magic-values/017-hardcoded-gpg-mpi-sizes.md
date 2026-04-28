---
title: "[MEDIUM] Hardcoded GPG MPI sizes for ECC keys in mod_gpg"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
The GPG module uses hardcoded MPI (Multi-Precision Integer) sizes for ED25519 (34 bytes) and P-256 (67 bytes) keys without named constants. These sizes depend on the key format and curve parameters.

**Location:** `components/mod_gpg/src/gpg.cpp:176-188`

## Impact
- **Clarity**: Magic numbers 34 and 67 don't explain "ED25519 MPI size" or "P-256 MPI size".
- **Maintainability**: If key format changes, these values must be found and updated.
- **Self-documentation**: Named constants would make the code more self-explanatory.

## Evidence

**Lines 176-189:**
```cpp
if (curve == CDC_CURVE_ED25519) {
    uint16_t bits = 256;
    if ((pubkey[0] & 0x80) == 0) bits = 255;
    mpi[0] = (bits >> 8) & 0xFF;
    mpi[1] = bits & 0xFF;
    memcpy(mpi + 2, pubkey, 32);
    mpi_len = 34;  // Magic number
} else {
    uint16_t bits = 520;  // Magic number for P-256
    mpi[0] = (bits >> 8) & 0xFF;
    mpi[1] = bits & 0xFF;
    memcpy(mpi + 2, pubkey, 65);  // 65 bytes for uncompressed P-256
    mpi_len = 67;  // Magic number
}
```

**Line 191:**
```cpp
uint8_t body[256];  // Magic buffer size
```

## Recommended Fix

1. **Define GPG MPI size constants** in `components/mod_gpg/include/mod_gpg/gpg_constants.h`:
    ```cpp
    #pragma once
    #include <cstdint>
    
    /**
     * \brief GPG/OpenPGP constants for MPI sizes and key formats
     * 
     * Based on:
     * - OpenPGP RFC 4880: MPI format (2 byte length + data)
     * - ED25519: 32-byte key + 2-byte length = 34 bytes
     * - P-256 (uncompressed): 65-byte key + 2-byte length = 67 bytes
     */
    namespace cdc::mod_gpg {
    namespace gpg {
        // ED25519 constants
        static constexpr size_t ED25519_KEY_SIZE = 32;
        static constexpr size_t ED25519_MPI_SIZE = 34;  // 2 bytes length + 32 bytes key
        
        // P-256 (secp256r1) constants
        static constexpr size_t P256_KEY_SIZE = 65;  // Uncompressed format (0x04 + 32 bytes X + 32 bytes Y)
        static constexpr size_t P256_MPI_SIZE = 67;  // 2 bytes length + 65 bytes key
        static constexpr size_t P256_BITS = 256;
        
        // SHA-1 fingerprint (OpenPGP v4)
        static constexpr size_t SHA1_DIGEST_SIZE = 20;
    }
    }
    ```

2. **Update usage** to use constants:
    ```cpp
    // Before:
    if (curve == CDC_CURVE_ED25519) {
        uint16_t bits = 256;
        if ((pubkey[0] & 0x80) == 0) bits = 255;
        memcpy(mpi + 2, pubkey, 32);
        mpi_len = 34;
    } else {
        uint16_t bits = 520;
        memcpy(mpi + 2, pubkey, 65);
        mpi_len = 67;
    }
    
    // After:
    if (curve == CDC_CURVE_ED25519) {
        uint16_t bits = 256;
        if ((pubkey[0] & 0x80) == 0) bits = 255;
        memcpy(mpi + 2, pubkey, gpg::ED25519_KEY_SIZE);
        mpi_len = gpg::ED25519_MPI_SIZE;
    } else {
        uint16_t bits = gpg::P256_BITS;
        memcpy(mpi + 2, pubkey, gpg::P256_KEY_SIZE);
        mpi_len = gpg::P256_MPI_SIZE;
    }
    ```

## References
- [OpenPGP RFC 4880](https://datatracker.ietf.org/doc/html/rfc4880): MPI format
- [ED25519](https://en.wikipedia.org/wiki/EdDSA): 32-byte keys
- [P-256](https://en.wikipedia.org/wiki/Elliptic_Curve_Digital_Signature_Algorithm): 65-byte uncompressed format
