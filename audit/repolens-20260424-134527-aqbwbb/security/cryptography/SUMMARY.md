# Cryptographic Implementation Audit Summary

## Overview

This document summarizes all cryptographic findings from the audit of the CDC Badge OS firmware codebase.

## Findings by Severity

### HIGH (1 finding)

1. **[HIGH] SHA-1 Used for OpenPGP Fingerprint Calculation**
   - Location: `components/mod_gpg/src/gpg.cpp`
   - SHA-1 is used for OpenPGP v4 fingerprint calculation. While spec-compliant, SHA-1 is cryptographically weak.
   - See: `001-sha1-openpgp-fingerprint.md`

### MEDIUM (3 findings)

2. **[MEDIUM] ECDH Shared Secret Not Cleared After Use**
    - Location: `components/mod_fido2/src/ctap2.cpp:2062-2133`
    - The ECDH shared secret (`ecdh_z`) and HKDF PRK (`prk`) are not cleared after use in `client_pin_compute_shared_secret()`.
    - See: `008-ecdh-shared-secret-not-cleared.md`

3. **[MEDIUM] Fixed Zero IV Used for AES-CBC in FIDO2 ClientPIN Protocol 1**
    - Location: `components/mod_fido2/src/ctap2.cpp:2172-2202`
    - Protocol 1 uses a fixed zero IV for AES-256-CBC. Protocol 2 correctly uses random IV.
    - See: `002-fido2-zero-iv-cbc.md`

4. **[MEDIUM] PIN Hash Enc Buffer Not Cleared After Use**
    - Location: `components/mod_fido2/src/ctap2.cpp:2353-2632`
    - The `pin_hash_enc`, `shared_secret`, and `decrypted_pin_hash` buffers are not cleared after use in `client_pin_get_pin_uv_auth_token_using_pin_with_permissions()`.
    - See: `011-pin-hash-enc-not-cleared.md`

### LOW (5 findings)

5. **[LOW] HMAC Verification Uses Non-Constant-Time memcmp**
   - Location: `components/mod_fido2/src/ctap2.cpp:2172-2202`
   - Protocol 1 uses a fixed zero IV for AES-256-CBC. Protocol 2 correctly uses random IV.
   - See: `002-fido2-zero-iv-cbc.md`

### LOW (5 findings)

5. **[LOW] HMAC Verification Uses Non-Constant-Time memcmp**
    - Location: `components/mod_fido2/src/ctap2.cpp:942,1572`
    - pin_uv_auth_param HMAC comparison uses `memcmp()` instead of constant-time comparison.
    - See: `007-hmac-memcmp-comparison.md`

6. **[LOW] AES-256-CBC Used Instead of GCM for FIDO2 PIN Protocol**
    - Location: `components/mod_fido2/src/ctap2.cpp:2145-2202`
    - CBC mode provides confidentiality but not authenticated encryption. HMAC provides integrity.
    - See: `004-fido2-cbc-instead-of-gcm.md`

7. **[LOW] PIN-Derived Key Salt Not Cleared After HKDF**
    - Location: `components/mod_gpg/src/GpgStorage.cpp:134-162`
    - The salt buffer (chip ID) is not cleared in `derive_key_from_pin()`.
    - See: `009-pin-derived-key-salt-not-cleared.md`

8. **[LOW] ClientPIN ECDH Key Not Freed on CTAP2 Reset**
    - Location: `components/mod_fido2/src/ctap2.cpp:1998-2013`
    - The ECDH key pair is initialized but never freed.
    - See: `010-clientpin-ecdh-key-not-freed.md`

9. **[LOW] PinManager Hash Buffers Not Cleared After Use**
    - Location: `components/cdc_core/src/PinManager.cpp:221-274`
    - The `fullHash` and `buffer` (salt+PIN) are not cleared after use.
    - See: `012-pinmanager-hash-buffers-not-cleared.md`

### INFO (1 finding)

10. **[INFO] SHA-1 Used as Default TOTP Algorithm**
    - Location: `components/mod_totp/src/TotpStore.cpp:448-456`
    - SHA-1 is the default algorithm for TOTP. SHA-256 is more modern.
    - See: `003-totp-default-sha1.md`

### REVIEW (2 findings)

10. **[INFO] Review Random Number Generation Usage Across All Modules**
    - Location: Multiple files
    - Verify all modules use the same secure RNG pattern.
    - See: `006-random-number-generation-review.md`

11. **[LOW] Verify All HMAC/Token Comparisons Use Constant-Time**
    - Location: Multiple files in `components/mod_fido2/` and `components/mod_gpg/`
    - Audit all HMAC/token comparisons for constant-time implementation.
    - See: `005-verify-constant-time-comparison.md`

## Key Observations

### Strengths

1. **Good RNG Usage**: The codebase uses TROPIC01 TRNG with ESP32 fallback for random number generation.
2. **Proper Key Clearing**: The GPG module properly clears key material using `mbedtls_platform_zeroize()`.
3. **Modern Algorithms**: AES-256, SHA-256, P-256, and Ed25519 are used for most operations.
4. **Authenticated Encryption**: AES-256-GCM is used for GPG DEC key encryption.

### Areas for Improvement

1. **Key Clearing**: Some intermediate secrets (ECDH shared secret, HKDF PRK, PIN hash buffers) are not cleared.
2. **Constant-Time Comparison**: Some HMAC comparisons use `memcmp()` instead of constant-time.
3. **CBC Mode**: FIDO2 ClientPIN uses CBC instead of GCM (spec-compliant but not optimal).
4. **SHA-1 Usage**: SHA-1 is used for OpenPGP v4 fingerprints and TOTP default.

## Recommendations

### Immediate Actions (High/Medium)

1. Add `mbedtls_platform_zeroize()` calls to clear ECDH shared secrets
2. Add `mbedtls_platform_zeroize()` calls to clear PIN hash buffers in `client_pin_get_pin_uv_auth_token_using_pin_with_permissions()`
3. Add documentation explaining SHA-1 usage for OpenPGP v4 spec compliance
4. Consider making SHA-256 the default TOTP algorithm

### Short-Term Actions (Low)

1. Replace `memcmp()` with constant-time comparison for HMAC verification
2. Add cleanup for ClientPIN ECDH key
3. Clear salt buffer in `derive_key_from_pin()`
4. Clear hash buffers in `PinManager::computeBadgeHash()` and `computeKdfHash()`

### Long-Term Actions (INFO/REVIEW)

1. Audit all modules for consistent RNG usage
2. Consider GCM mode for future protocol versions
3. Plan migration to OpenPGP v5 fingerprints for new keys

## References

- [NIST SP 800-131A Rev 2](https://csrc.nist.gov/publications/detail/sp/800-131a/rev-2/final)
- [FIDO CTAP2 Specification](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html)
- [OpenPGP RFC 4880](https://tools.ietf.org/html/rfc4880)
- [OpenPGP v5 RFC 9580](https://tools.ietf.org/html/rfc9580)
- [Mbed TLS Documentation](https://tls.mbed.org/)
