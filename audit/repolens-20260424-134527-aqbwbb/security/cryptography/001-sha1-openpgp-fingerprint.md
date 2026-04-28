---
title: "[HIGH] SHA-1 Used for OpenPGP Fingerprint Calculation"
severity: HIGH
domain: cryptographic-implementation
lens: cdc-badge-os
labels:
  - sha1
  - openpgp
  - fingerprint
---

## Summary

SHA-1 is used for OpenPGP v4 fingerprint calculation in the GPG module. While this follows the OpenPGP v4 specification, SHA-1 is cryptographically weak and susceptible to collision attacks. The code should clearly document this is for spec compliance and consider making v5 fingerprints (SHA-256) the default for new keys.

**Location:** `components/mod_gpg/src/gpg.cpp:210-217`

## Impact

- SHA-1 collision attacks are practical (first demonstrated in 2017)
- While fingerprints are used for identification rather than authentication, relying on a weak hash reduces overall security posture
- Future OpenPGP implementations may deprecate v4 fingerprints
- New keys should use stronger v5 fingerprints where possible

## Evidence

File: `components/mod_gpg/src/gpg.cpp` (line 6 includes `<mbedtls/sha1.h>`)

```cpp
static void calculate_fingerprint(const uint8_t *public_key, size_t key_len,
                                  uint8_t *sha, bool v5 = false) {
    if (!v5) {
        // OpenPGP v4 fingerprint (SHA-1)
        mbedtls_sha1_context sha1;
        mbedtls_sha1_init(&sha1);
        mbedtls_sha1_starts(&sha1);

        // Prefix for v4 fingerprint
        uint8_t prefix[] = {0x99, 0x00};
        prefix[1] = (key_len >> 8) & 0xFF;
        prefix[2] = key_len & 0xFF;

        mbedtls_sha1_update(&sha1, prefix, sizeof(prefix));
        mbedtls_sha1_update(&sha1, public_key, key_len);
        mbedtls_sha1_finish(&sha1, sha);
        mbedtls_sha1_free(&sha1);
    } else {
        // OpenPGP v5 fingerprint (SHA-256)
        // ... implementation using SHA-256
    }
}
```

The v5 implementation (lines 230-288) uses SHA-256 correctly:
```cpp
mbedtls_sha256_context sha256;
mbedtls_sha256_init(&sha256);
mbedtls_sha256_starts(&sha256, 0);  // SHA-256 (not 224)
mbedtls_sha256_update(&sha256, prefix, sizeof(prefix));
mbedtls_sha256_update(&sha256, body, body_len);
mbedtls_sha256_finish(&sha256, sha);
mbedtls_sha256_free(&sha256);
```

## Recommended Fix

1. **Add documentation comment** before the SHA-1 block explaining this is for OpenPGP v4 spec compatibility:
   ```cpp
   // OpenPGP v4 fingerprint uses SHA-1 per RFC 4880 for backward compatibility
   // Consider using v5 fingerprints (SHA-256) for new keys
   ```

2. **Make v5 fingerprints the default** for newly generated keys:
   - Update `GpgStorage::generateKey()` to use `calculate_fingerprint(..., true)` for new keys
   - Keep v4 support for importing/displaying existing v4 keys

3. **Add a configuration option** to allow users to choose v4 vs v5 fingerprints

## References

- [RFC 4880 - OpenPGP Message Format](https://tools.ietf.org/html/rfc4880)
- [RFC 9580 - OpenPGP Version 5](https://tools.ietf.org/html/rfc9580)
- [SHAttered Attack - First SHA-1 Collision](https://shattered.it/)
- [NIST SP 800-131A Rev 2 - Transitioning the Use of Cryptographic Algorithms](https://csrc.nist.gov/publications/detail/sp/800-131a/rev-2/final)
