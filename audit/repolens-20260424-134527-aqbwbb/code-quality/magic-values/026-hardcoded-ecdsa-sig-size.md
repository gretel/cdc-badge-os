---
title: "[MEDIUM] Hardcoded ECDSA signature size (64 bytes)"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
The OpenPGP implementation uses hardcoded value `64` for ECDSA signature size (R || S, 32 bytes each) throughout `components/mod_gpg/src/openpgp/openpgp.cpp`. This appears in multiple places without a named constant.

**Locations:** Lines 77, 83, 1235, 1259

## Impact
- **Clarity**: Value `64` doesn't explain it's ECDSA P-256 signature format (R(32) || S(32)).
- **Maintainability**: If ECDSA is replaced with another algorithm, all occurrences must be found.
- **Consistency**: Multiple occurrences (buffer declaration, validation, response building).

## Evidence

**Lines 77, 83 (function signature and initialization):**
```cpp
/**
 * \brief Sign hash directly using TROPIC01
 * \param sig Output 64-byte signature buffer.
 */
size_t sig_len = 64;
```

**Line 1235 (buffer declaration):**
```cpp
uint8_t signature[64];  // R (32 bytes) || S (32 bytes)
```

**Line 1259 (response building):**
```cpp
return apdu_build_response(resp, resp_max, signature, 64, SW_OK);
```

The value `64` is used for:
- Buffer declaration for ECDSA signatures
- Signature length validation
- Response building

Note: This is specific to ECDSA with P-256 curve. EdDSA (Ed25519) also produces 64-byte signatures but for different reasons.

## Recommended Fix

1. **Define named constants** for signature sizes:
```cpp
/** \brief ECDSA P-256 signature size (R(32) || S(32)) */
static constexpr size_t ECDSA_P256_SIG_SIZE = 64;

/** \brief EdDSA (Ed25519) signature size */
static constexpr size_t EDDSA_SIG_SIZE = 64;

/** \brief Generic ECDSA signature size (works for P-256 and Ed25519) */
static constexpr size_t ECDSA_SIG_SIZE = 64;
```

2. **Update usage** to use constants:
```cpp
// Before:
uint8_t signature[64];  // R (32 bytes) || S (32 bytes)
return apdu_build_response(resp, resp_max, signature, 64, SW_OK);

// After:
uint8_t signature[ECDSA_SIG_SIZE];  // R (32 bytes) || S (32 bytes)
return apdu_build_response(resp, resp_max, signature, ECDSA_SIG_SIZE, SW_OK);
```

3. **Add documentation** explaining the format:
```cpp
/**
 * \brief ECDSA signature size (P-256 and Ed25519)
 * 
 * Format: R (32 bytes) || S (32 bytes)
 * 
 * P-256 (ECDSA): Two 256-bit integers encoded as big-endian
 * Ed25519 (EdDSA): 32-byte signature + 32-byte public key
 * 
 * Reference: SEC 1 Section 4.1.6, RFC 8032 Section 3.2
 */
static constexpr size_t ECDSA_SIG_SIZE = 64;
```

## References
- [SEC 1 - Elliptic Curve Cryptography](https://www.secg.org/sec1-v2.pdf): Section 4.1.6
- [RFC 8032 - EdDSA](https://datatracker.ietf.org/doc/html/rfc8032#section-3.2)
- [OpenPGP Smart Card Application 3.4.1](https://github.com/ANSSI-FR/openpgp-card)
- Related finding: `017-hardcoded-gpg-mpi-sizes.md`
