---
title: "[MEDIUM] Weak Cryptographic Algorithm: SHA1 Used for OpenPGP Fingerprints"
severity: MEDIUM
domain: security
lens: toolgate/security-sast
labels:
  - "audit:toolgate/security-sast"
  - "cwe-327"
  - "weak-crypto"
---

## Summary
The GPG module uses SHA1 (a cryptographically weak hash algorithm) for calculating OpenPGP v4 fingerprints. The SHA1 algorithm has known collision vulnerabilities and is deprecated for most cryptographic uses.

**Location:** `components/mod_gpg/src/gpg.cpp` lines 209-216

**Vulnerable Code:**
```cpp
uint8_t sha[20];
mbedtls_sha1_context sha1;
mbedtls_sha1_init(&sha1);
mbedtls_sha1_starts(&sha1);
mbedtls_sha1_update(&sha1, prefix, sizeof(prefix));
mbedtls_sha1_update(&sha1, body, body_len);
mbedtls_sha1_finish(&sha1, sha);
mbedtls_sha1_free(&sha1);
```

## Impact
- **Security Risk**: SHA1 has known collision vulnerabilities (SHAttered attack, 2017)
- **Standards Compliance**: OpenPGP v4 fingerprints use SHA1 by specification, but future compatibility may require migration
- **Cryptographic Agility**: Hard-coded SHA1 makes it harder to upgrade to stronger algorithms later

## Evidence
**File:** `components/mod_gpg/src/gpg.cpp`
**Function:** `calculate_fingerprint()` (lines 161-219)
**CWE:** CWE-327 (Use of a Risky Cryptographic Algorithm)

The function uses `mbedtls_sha1_*` functions to compute 20-byte fingerprints for OpenPGP v4 format. While this is the OpenPGP v4 specification, SHA1 is considered weak for general cryptographic use.

Note: OpenPGP v5 fingerprints (lines 230-288) correctly use SHA256.

## Recommended Fix
1. **Document the rationale**: Add a comment explaining that SHA1 is used per OpenPGP v4 specification (RFC 4880)
2. **Consider future migration**: Add a configuration option or feature flag for SHA256-based fingerprints
3. **Track OpenPGP updates**: Monitor IETF OpenPGP working group for SHA1 deprecation timeline

Example comment to add:
```cpp
// SHA1 is used per OpenPGP v4 spec (RFC 4880).
// For v5 fingerprints, SHA256 is used (see calculate_fingerprint_v5()).
```

## References
- [CWE-327: Use of a Risky Cryptographic Algorithm](https://cwe.mitre.org/data/definitions/327.html)
- [OpenPGP v4 Fingerprint Specification (RFC 4880)](https://datatracker.ietf.org/doc/html/rfc4880#section-12.4)
- [SHAttered SHA1 Collision Attack (2017)](https://shattered.io/)
- [Mbed TLS SHA1 Documentation](https://github.com/Mbed-TLS/mbedtls)
