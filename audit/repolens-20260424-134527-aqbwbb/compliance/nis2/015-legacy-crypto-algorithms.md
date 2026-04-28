---
title: "[MEDIUM] Legacy cryptographic algorithms in third-party crypto library (RC4, RIPEMD-160)"
severity: MEDIUM
domain: encryption
lens: nis2
labels:
  - "audit:compliance/nis2"
---

## Summary
The trezor_crypto library (third_party/libtropic/vendor/trezor_crypto/) includes legacy cryptographic algorithms: RC4 stream cipher and RIPEMD-160 hash. While these may not be actively used in the CDC Badge OS codebase, their presence increases the attack surface and risk of accidental usage. NIS2 Article 20 requires "state-of-the-art" encryption, and both RC4 and RIPEMD-160 are considered deprecated.

## Impact
**NIS2 Art. 20 Cryptographic Standards Gap**:
- **RC4**: Known biases in keystream, practical attacks exist (Barak et al. 2015), RFC 7465 prohibits its use
- **RIPEMD-160**: 160-bit output may be insufficient for long-term security, SHA-256/SHA-3 preferred
- Risk of accidental usage if developers are unaware of which algorithms are "safe"
- Larger codebase = larger attack surface

## Evidence

1. **RC4 implementation present**:
   - File: `third_party/libtropic/vendor/trezor_crypto/rc4.c:24-50`
   ```c
   void rc4_init(RC4_CTX *ctx, const uint8_t *key, size_t length) {
       // KSA (Key-Scheduling Algorithm)
       for (size_t i = 0; i < 256; i++) {
           ctx->S[i] = i;
       }
       for (size_t i = 0; i < 256; i++) {
           j += ctx->S[i] + key[i % length];
           rc4_swap(ctx, i, j);
       }
   }
   
   void rc4_encrypt(RC4_CTX *ctx, uint8_t *buffer, size_t length) {
       // PRGA (Pseudo-Random Generation Algorithm)
       buffer[idx] ^= K;  // XOR with keystream
   }
   ```
   - RC4 is deprecated (RFC 7465, RFC 8052)
   - Known biases make it vulnerable to statistical attacks

2. **RIPEMD-160 implementation present**:
   - File: `third_party/libtropic/vendor/trezor_crypto/ripemd160.c`
   - 160-bit hash, designed in 1996
   - No collision attacks known, but SHA-256 preferred for new applications

3. **No algorithm usage documentation**:
   - No README explaining which algorithms are "safe to use"
   - No deprecation notices in trezor_crypto headers
   - Developers may accidentally use RC4 for new features

4. **Crypto library includes many algorithms**:
   - File: `third_party/libtropic/vendor/trezor_crypto/`
   - Contains: RC4, RIPEMD-160, DES (in aes/), SHA-1, MD5, etc.
   - No clear guidance on which to use for new features

## Recommended Fix

1. **Add deprecation warnings to legacy algorithms**:
   ```c
   // third_party/libtropic/vendor/trezor_crypto/rc4.h
   #ifndef _TREZOR_RC4_H
   #define _TREZOR_RC4_H
   
   /**
    * @deprecated RC4 is deprecated. Use AES-CTR or ChaCha20 instead.
    * RFC 7465 prohibits RC4 for TLS.
    */
   typedef struct {
       uint8_t S[256];
       uint8_t i, j;
   } RC4_CTX;
   
   __attribute__((deprecated("Use AES-CTR or ChaCha20 instead")))
   void rc4_init(RC4_CTX *ctx, const uint8_t *key, size_t length);
   
   __attribute__((deprecated("Use AES-CTR or ChaCha20 instead")))
   void rc4_encrypt(RC4_CTX *ctx, uint8_t *buffer, size_t length);
   
   #endif
   ```

2. **Create cryptographic guidelines document**:
   - File: `docs/CRYPTO_GUIDELINES.md`
   ```markdown
   ## Approved Algorithms for CDC Badge OS
   
   ### Hash Functions
   - SHA-256 (preferred)
   - SHA-512
   - SHA-3 (if available)
   
   ### Encryption
   - AES-256-GCM (preferred for symmetric)
   - AES-256-CTR
   - ChaCha20-Poly1305
   
   ### Key Derivation
   - HKDF-SHA256
   - PBKDF2-SHA256
   
   ### Legacy (Use with caution)
   - SHA-1 (only for compatibility)
   - RIPEMD-160 (only for Bitcoin compatibility)
   
   ### Deprecated (Avoid)
   - RC4 (known weaknesses)
   - MD5 (collision attacks)
   - DES (56-bit key too small)
   ```

3. **Add static analysis rule for crypto usage**:
   - Configure check-crypto tool or script
   - Flag usage of deprecated algorithms in new code
   - Add CI check to prevent accidental RC4/MD5 usage

4. **Document algorithm selection rationale**:
   - Add comments in code explaining why specific algorithms were chosen
   - Reference NIS2 requirements for cryptographic standards
   - Link to NIST SP 800-131A (transitional algorithms)

5. **Consider pruning unused algorithms**:
   - If RC4/RIPEMD-160 are not used, remove them from build
   - Reduce codebase size and attack surface
   - Use build flags: `-DENABLE_RC4=0`

## References
- [NIS2 Directive Art. 20 - Encryption](https://eur-lex.europa.eu/eli/dir/2022/2555/oj)
- [RFC 7465 - Prohibiting RC4](https://datatracker.ietf.org/doc/html/rfc7465)
- [RFC 8052 - PQC Hash Functions](https://datatracker.ietf.org/doc/html/rfc8052)
- [NIST SP 800-131A Rev. 2 - Transitional Algorithms](https://csrc.nist.gov/publications/detail/sp/800-131a/rev-2/final)
- [Trezor Crypto README](third_party/libtropic/vendor/trezor_crypto/README.md)

</content>