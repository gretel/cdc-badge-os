---
title: "[MEDIUM] Hardcoded ECDSA hash size (32 bytes) and signature length (64 bytes)"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
Magic values `32` (hash size) and `64` (signature length) are used directly in `components/cdc_hal/src/Tropic01Element.cpp` for ECDSA operations. While these are standard cryptographic values (SHA-256 digest, P-256 signature), they should be defined as named constants for clarity and maintainability.

**Locations:**
- `components/cdc_hal/src/Tropic01Element.cpp:411` - `pubKey, 64, &ltCurve, &ltOrigin`
- `components/cdc_hal/src/Tropic01Element.cpp:486` - `hashLen != 32`
- `components/cdc_hal/src/Tropic01Element.cpp:502` - `*sigLen = TR01_ECDSA_EDDSA_SIGNATURE_LENGTH;  // Always 64 bytes (R,S)`

## Impact
- **Clarity**: The meaning of `64` and `32` is only clear from context or comments.
- **Maintainability**: If the codebase needs to support different curve sizes (e.g., P-384), these magic numbers would need to be found and updated.
- **Documentation**: One instance has a comment (`// Always 64 bytes (R,S)`), but others rely on implicit knowledge.

## Evidence

**Line 411 - Public key buffer size:**
```cpp
lt_ret_t ret = lt_ecc_key_read(&handle_, static_cast<lt_ecc_slot_t>(slot),
                                pubKey, 64, &ltCurve, &ltOrigin);
```

**Line 486 - Hash length validation:**
```cpp
if (slot >= ECC_SLOT_COUNT || !hash || hashLen != 32 || !sig || !sigLen) {
```

**Line 480 - Comment explains the magic value:**
```cpp
* \param sig Destination buffer for 64-byte raw `(R,S)` signature.
```

**Line 502 - Already uses a named constant:**
```cpp
*sigLen = TR01_ECDSA_EDRSA_SIGNATURE_LENGTH;  // Always 64 bytes (R,S)
```

Note: `TR01_ECDSA_EDDSA_SIGNATURE_LENGTH` is already defined, but the value `32` for hash size is still a magic number.

## Recommended Fix

1. Define named constants for cryptographic sizes near other TROPIC01 constants:
```cpp
/** \brief Cryptographic size constants. */
static constexpr size_t SHA256_DIGEST_SIZE = 32;
static constexpr size_t P256_PUBLIC_KEY_SIZE = 64;
static constexpr size_t P256_SIGNATURE_SIZE = 64;
```

2. Update the code to use these constants:
```cpp
// Line 411
lt_ret_t ret = lt_ecc_key_read(&handle_, static_cast<lt_ecc_slot_t>(slot),
                                pubKey, P256_PUBLIC_KEY_SIZE, &ltCurve, &ltOrigin);

// Line 486
if (slot >= ECC_SLOT_COUNT || !hash || hashLen != SHA256_DIGEST_SIZE || !sig || !sigLen) {
```

3. Consider using the existing `TR01_ECDSA_EDDSA_SIGNATURE_LENGTH` consistently or replace it with a locally-defined constant for clarity.

## References
- [NIST P-256 Curve Specification](https://csrc.nist.gov/projects/elliptic-curve-cryptography)
- [SHA-256 Specification](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf)
- `components/cdc_hal/src/Tropic01Element.cpp` - TROPIC01 secure element implementation
