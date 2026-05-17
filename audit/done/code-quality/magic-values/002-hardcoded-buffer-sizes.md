---
title: "[MEDIUM] Hardcoded buffer sizes in local variable declarations"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "magic-values"
---

## Summary
Multiple hardcoded numeric array sizes are used in local variable declarations throughout the codebase. These sizes represent cryptographic buffer dimensions, string storage, and temporary data buffers but lack named constants with explanatory comments.

**Files affected:**
- `components/cdc_core/src/PinManager.cpp:224` - `uint8_t fullHash[32]`
- `components/cdc_core/src/AttestationKeyService.cpp:114,145,148` - `uint8_t pubkey[64]`, `uint8_t hash[32]`, `uint8_t stored[32]`
- `components/cdc_core/src/KeyFingerprint.cpp:48,86` - `uint8_t hash[32]`, `uint8_t pubkey[64]`
- `components/mod_totp/src/TotpModule.cpp:229,554,561` - `char secret[128]`, `char (*s_listLabels)[24]`
- `components/mod_totp/src/TotpStore.cpp:410` - `uint8_t hmac[64]`
- `components/cdc_log/src/cdc_log.cpp:161,188,325` - `char buf[256]`
- `components/mod_gpg/src/openpgp/openpgp.cpp:456,730,1600` - `uint8_t inner[128]`, `uint8_t data[128]`, `uint8_t tlv_data[128]`
- `components/mod_gpg/src/openpgp/ccid.cpp:154` - `char hex[128]`
- `components/mod_gpg/src/GpgModule.cpp:227` - `char name[64]`

**Already defined but inconsistently used:**
- `PinManager.h` has well-defined constants: `BADGE_HASH_SIZE = 16`, `KDF_HASH_SIZE = 32`, `SALT_SIZE = 8`
- `TotpStore.h` has: `NAME_LEN = 16`, `ISSUER_LEN = 32`, `SECRET_LEN = 32`
- `mod_nvsedit` has: `MAX_NAMESPACES = 32`, `MAX_KEYS = 48`

## Impact
- **Maintainability**: Changing buffer sizes requires searching through implementation files
- **Safety**: Magic sizes may not match the actual requirements (e.g., SHA-256 is 32 bytes, but why 64 for HMAC?)
- **Readability**: `uint8_t hmac[64]` doesn't explain that this is HMAC-SHA256 output + padding
- **Consistency**: Some files use constants (PinManager.cpp uses `BADGE_HASH_SIZE`), others use magic numbers

## Evidence
```cpp
// components/cdc_core/src/PinManager.cpp:224
uint8_t fullHash[32];  // Magic: Should use mbedtls_sha256() result size constant

// components/cdc_core/src/AttestationKeyService.cpp:114,145
uint8_t pubkey[64] = {};  // Magic: X25519 public key size (32) + encoding overhead?
uint8_t hash[32] = {};    // Magic: SHA-256 size

// components/mod_totp/src/TotpModule.cpp:229,554
char secret[128] = {};    // Magic: Base32-encoded secret max size?
static char (*s_listLabels)[24] = nullptr;  // Magic: Label length + null terminator?

// components/cdc_log/src/cdc_log.cpp:161,188,325
char buf[256];  // Magic: Log message buffer size

// components/mod_gpg/src/openpgp/openpgp.cpp:456,730
uint8_t inner[128];   // Magic: TLV container size?
uint8_t data[128];    // Magic: Maximum TLV payload?
```

## Recommended Fix
1. **Create centralized cryptographic constant definitions** in `components/cdc_core/include/cdc_core/crypto_constants.h`:
   ```cpp
   // Cryptographic buffer sizes
   static constexpr size_t SHA256_DIGEST_SIZE = 32;
   static constexpr size_t SHA256_CTX_SIZE = sizeof(mbedtls_sha256_context);
   static constexpr size_t HMAC_SHA256_SIZE = 32;
   static constexpr size_t X25519_KEY_SIZE = 32;
   static constexpr size_t ECC_PUBLIC_KEY_ENCODED = 64;  // Compressed format
   
   // String buffer sizes
   static constexpr size_t LOG_BUFFER_SIZE = 256;
   static constexpr size_t SECRET_BUFFER_SIZE = 128;  // Base32 TOTP secret
   static constexpr size_t LABEL_BUFFER_SIZE = 24;    // Display label + null
   ```

2. **Replace magic sizes** in implementation files with the constants

3. **Add comments** explaining the rationale:
   ```cpp
   uint8_t fullHash[SHA256_DIGEST_SIZE];  // SHA-256 produces 32-byte digest
   ```

4. **Ensure consistency** across all modules (GPG, TOTP, FIDO2, etc.)

## References
- [SHA-256 specification (FIPS 180-4)](https://csrc.nist.gov/publications/detail/fips/180/4/final)
- [RFC 4253 - SSH Transport Layer (key sizes)](https://datatracker.ietf.org/doc/html/rfc4253)
- [RFC 6238 - TOTP (secret encoding)](https://datatracker.ietf.org/doc/html/rfc6238)
