---
title: "[MEDIUM] TOTP module defaults to SHA1 algorithm (deprecated cryptographic hash)"
severity: MEDIUM
domain: encryption
lens: nis2
labels:
  - "audit:compliance/nis2"
---

## Summary
The TOTP module uses SHA1 as the default algorithm when no algorithm is specified or when parsing fails. SHA1 is a deprecated cryptographic hash algorithm with known collision vulnerabilities. While TOTP with SHA1 still provides some security, NIS2 requires use of "state-of-the-art" encryption (Art. 20). The default fallback to SHA1 may lead to weaker security than intended.

## Impact
**NIS2 Art. 20 Encryption Gap**:
- SHA1 has known collision vulnerabilities (2017 SHAttered attack)
- Defaulting to SHA1 may result in weaker TOTP security
- Users may not be aware they're using SHA1
- Industry trend is toward SHA256 or better for TOTP

## Evidence
1. **SHA1 is the default in TotpAlgorithm enum**:
   - File: `components/mod_totp/include/mod_totp/TotpStore.h:9-13`
   ```cpp
   enum class TotpAlgorithm : uint8_t {
       SHA1 = 0,      // Default (first value)
       SHA256 = 1,
       SHA512 = 2
   };
   ```

2. **SHA1 used as fallback when algorithm not specified**:
   - File: `components/mod_totp/src/TotpModule.cpp:139-144`
   ```cpp
   if (!token || !*token) return static_cast<uint8_t>(TotpAlgorithm::SHA1);
   // ...
   if (strcmp(buf, "sha1") == 0) return static_cast<uint8_t>(TotpAlgorithm::SHA1);
   ```

3. **SHA1 used as default in wizard initialization**:
   - File: `components/mod_totp/src/TotpModule.cpp:747`
   ```cpp
   s_wizard.algorithm = static_cast<uint8_t>(TotpAlgorithm::SHA1);
   ```

4. **SHA1 used as fallback in HMAC computation**:
   - File: `components/mod_totp/src/TotpStore.cpp:452-454`
   ```cpp
   default:
       mdType = MBEDTLS_MD_SHA1;  // Fallback to SHA1
       expectedLen = 20;
       break;
   ```

5. **TotpAlgorithm enum uses uint8_t for storage**:
   - File: `components/mod_totp/include/mod_totp/TotpStore.h:22`
   - Algorithm stored as byte in R-Memory, defaults to 0 (SHA1)

## Recommended Fix
1. **Change default to SHA256**:
   - Update `TotpAlgorithm` enum to make SHA256 the default:
   ```cpp
   enum class TotpAlgorithm : uint8_t {
       SHA256 = 0,    // Default (changed from SHA1)
       SHA1 = 1,      // Legacy support
       SHA512 = 2
   };
   ```

2. **Update fallback logic**:
   - Change default case in `TotpStore.cpp:452` to use SHA256
   - Update wizard initialization to use SHA256

3. **Add migration path for existing TOTP entries**:
   - Detect existing SHA1 entries on first boot
   - Offer migration to SHA256 with user confirmation
   - Store migration status in NVS

4. **Add deprecation warning for SHA1**:
   - When user selects SHA1, display warning about deprecated algorithm
   - Suggest SHA256 as preferred option

5. **Document algorithm choice**:
   - Update `docs/GPG.md` or create `docs/TOTP.md` with algorithm rationale
   - Explain why SHA256 is preferred

## References
- [NIS2 Directive Art. 20 - Encryption](https://eur-lex.europa.eu/eli/dir/2022/2555/oj)
- [RFC 6238 - TOTP Algorithm](https://datatracker.ietf.org/doc/html/rfc6238)
- [RFC 4226 - HOTP Algorithm](https://datatracker.ietf.org/doc/html/rfc4226)
- [NIST SP 800-106 - Randomized Hash Mode for SHA-1](https://csrc.nist.gov/publications/detail/sp/800-106/final)
- [SHAttered Attack on SHA-1](https://shattered.io/)
- [Google Authenticator SHA256 support](https://github.com/google/google-authenticator)
