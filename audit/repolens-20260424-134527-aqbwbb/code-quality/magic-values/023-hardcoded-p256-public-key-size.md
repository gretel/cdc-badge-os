---
title: "[MEDIUM] Hardcoded P-256 public key size (65 bytes)"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
The OpenPGP implementation uses hardcoded value `65` for P-256 uncompressed public key size throughout `components/mod_gpg/src/openpgp/openpgp.cpp`. This appears in 10+ places without a named constant.

**Locations:** Lines 55, 330, 845, 1226, 1373, 1490, 1551, 1580, 1588

## Impact
- **Maintainability**: If P-256 is replaced or multiple curves supported, all occurrences must be found.
- **Clarity**: Value `65` doesn't explain it's P-256 uncompressed format (0x04 || X(32) || Y(32)).
- **Consistency**: 10+ occurrences scattered across different functions.

## Evidence

**Line 55 (validation helper):**
```cpp
return max_len >= 65;
```

**Lines 330, 845, 1226, 1551 (buffer declarations):**
```cpp
uint8_t pubkey[65];
```

**Lines 1372-1373 (validation):**
```cpp
// Verify public key length (65 bytes for uncompressed P-256)
if (pubkey_len != 65 || p + pubkey_len > end) {
```

**Lines 1580-1588 (ECDH computation):**
```cpp
// For ECDSA/ECDH (P-256): pubkey is 65 bytes (04 || X || Y) uncompressed
uint8_t pubkey_with_prefix[65];
...
memcpy(pubkey_with_prefix, pubkey, 65);
```

**Line 1490 (key generation):**
```cpp
uint8_t pubkey_gen[65];
```

## Recommended Fix

1. **Define a named constant** near other OpenPGP constants:
```cpp
/** \brief P-256 uncompressed public key size (0x04 || X(32) || Y(32)) */
static constexpr size_t P256_PUBKEY_SIZE = 65;

/** \brief P-256 compressed public key size (0x02/0x03 || X(32)) */
static constexpr size_t P256_PUBKEY_COMPRESSED_SIZE = 33;
```

2. **Replace all occurrences** with the constant:
```cpp
// Before:
uint8_t pubkey[65];
if (pubkey_len != 65 || p + pubkey_len > end) {

// After:
uint8_t pubkey[P256_PUBKEY_SIZE];
if (pubkey_len != P256_PUBKEY_SIZE || p + pubkey_len > end) {
```

3. **Add context** for future maintainers:
```cpp
/**
 * \brief P-256 uncompressed public key format
 * 
 * Structure: 0x04 (uncompressed) || X coordinate (32 bytes) || Y coordinate (32 bytes)
 * Used by: SEC 1 standard, OpenPGP, FIDO2
 * Reference: https://www.secg.org/sec1-v2.pdf
 */
static constexpr size_t P256_PUBKEY_SIZE = 65;
```

## References
- [SEC 1 - Elliptic Curve Cryptography](https://www.secg.org/sec1-v2.pdf): Section 2.3.3
- [OpenPGP Smart Card Application 3.4.1](https://github.com/ANSSI-FR/openpgp-card)
- [FIDO2 CTAP2 Specification](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html)
