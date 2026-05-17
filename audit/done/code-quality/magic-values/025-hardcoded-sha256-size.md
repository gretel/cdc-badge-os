---
title: "[MEDIUM] Hardcoded SHA-256 hash size (32 bytes)"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
The OpenPGP implementation uses hardcoded value `32` for SHA-256 hash size throughout `components/mod_gpg/src/openpgp/openpgp.cpp`. This appears in multiple places including ECDSA signature validation, private key storage, and shared secret computation.

**Locations:** Lines 53, 1240, 1389, 1397

## Impact
- **Clarity**: Value `32` doesn't explain it's SHA-256 output size (or P-256 private key size).
- **Maintainability**: If hash algorithm changes (e.g., SHA-512), all occurrences must be found.
- **Consistency**: Multiple contexts (hash size, key size, secret size) all use the same magic number.

## Evidence

**Line 53 (hash validation):**
```cpp
return max_len >= 32;
```

**Lines 1239-1241 (ECDSA signature):**
```cpp
// ECDSA: sign the hash (expected to be SHA-256, 32 bytes)
if (apdu->lc != 32) {
    ESP_LOGW(TAG, "ECDSA expects 32-byte hash, got %d", apdu->lc);
}
```

**Line 1389 (private key storage):**
```cpp
uint8_t dec_privkey[32];  // P-256 private key (scalar)
```

**Line 1397 (shared secret):**
```cpp
uint8_t shared_secret[32];  // ECDH output (32 bytes for P-256)
```

Note: The value `32` is used for multiple cryptographic concepts:
- SHA-256 hash output size
- P-256 private key size (scalar)
- P-256 shared secret size

## Recommended Fix

1. **Define named constants** for cryptographic sizes:
```cpp
/** \brief SHA-256 hash output size in bytes */
static constexpr size_t SHA256_SIZE = 32;

/** \brief P-256 private key size (scalar, 256 bits) */
static constexpr size_t P256_PRIVKEY_SIZE = 32;

/** \brief P-256 ECDH shared secret size */
static constexpr size_t P256_SHARED_SECRET_SIZE = 32;
```

2. **Update usage** with context-specific constants:
```cpp
// Before:
if (apdu->lc != 32) {
    ESP_LOGW(TAG, "ECDSA expects 32-byte hash, got %d", apdu->lc);
}

// After:
if (apdu->lc != SHA256_SIZE) {
    ESP_LOGW(TAG, "ECDSA expects %zu-byte hash, got %d", SHA256_SIZE, apdu->lc);
}

// Before:
uint8_t dec_privkey[32];
uint8_t shared_secret[32];

// After:
uint8_t dec_privkey[P256_PRIVKEY_SIZE];
uint8_t shared_secret[P256_SHARED_SECRET_SIZE];
```

3. **Add documentation** explaining the distinction:
```cpp
/**
 * \brief SHA-256 hash output size
 * 
 * Used for: ECDSA signatures, FIDO2 client data hash, general hashing
 * Reference: FIPS 180-4 (SHA-256 specification)
 */
static constexpr size_t SHA256_SIZE = 32;
```

## References
- [FIPS 180-4 - SHA-256 Specification](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf)
- [SEC 1 - Elliptic Curve Cryptography](https://www.secg.org/sec1-v2.pdf)
- Related finding: `023-hardcoded-p256-public-key-size.md`
