---
title: "[MEDIUM] SHA-1 Used as Default TOTP Algorithm"
severity: MEDIUM
domain: cryptographic-implementation
lens: cdc-badge-os
labels:
  - sha1
  - totp
  - hmac
---

## Summary

The TOTP implementation uses SHA-1 as the default algorithm when no algorithm is specified. While TOTP with SHA-1 is still considered secure for 2FA (due to the short-lived nature of codes and HMAC construction), modern best practices recommend SHA-256 or SHA-512 as the default.

**Location:** `components/mod_totp/src/TotpStore.cpp:448-456`

## Impact

- **Industry trend**: Most modern TOTP services use SHA-256 or SHA-512 as default
- **SHA-1 deprecation**: SHA-1 is being phased out across many cryptographic applications
- **Future-proofing**: New accounts should use stronger algorithms by default
- **Backward compatibility**: SHA-1 support should be kept for existing accounts

Note: The impact is limited because:
1. TOTP uses HMAC construction, which is more resilient to hash weaknesses
2. TOTP codes are short-lived (30 seconds)
3. The code supports SHA-256 and SHA-512, just not as default

## Evidence

File: `components/mod_totp/src/TotpStore.cpp`

```cpp
static mbedtls_md_type_t get_md_type_for_algo(TotpAlgorithm algo, size_t *expectedLen) {
    mbedtls_md_type_t mdType;
    switch (algo) {
        case TotpAlgorithm::SHA256:
            mdType = MBEDTLS_MD_SHA256;
            expectedLen = 32;
            break;
        case TotpAlgorithm::SHA512:
            mdType = MBEDTLS_MD_SHA512;
            expectedLen = 64;
            break;
        default:
            mdType = MBEDTLS_MD_SHA1;  // Default is SHA-1
            expectedLen = 20;
            break;
    }
    return mdType;
}
```

The TOTP structure defines the algorithm:
```cpp
typedef enum {
    SHA1 = 0,
    SHA256 = 1,
    SHA512 = 2
} TotpAlgorithm;
```

## Recommended Fix

1. **Change default algorithm** to SHA-256 for new accounts:
   ```cpp
   default:
       mdType = MBEDTLS_MD_SHA256;  // Default to SHA-256 for new accounts
       expectedLen = 32;
       break;
   ```

2. **Update the default algorithm enum** value:
   ```cpp
   typedef enum {
       SHA256 = 0,  // New default
       SHA1 = 1,    // Legacy support
       SHA512 = 2
   } TotpAlgorithm;
   ```

3. **Add documentation** explaining the default:
   ```cpp
   // Default to SHA-256 for new accounts. SHA-1 is kept for backward
   // compatibility with existing TOTP accounts.
   ```

4. **Update any command-line interface** that creates new TOTP accounts to explicitly use SHA-256

## References

- [RFC 6238 - TOTP: Time-Based One-Time Password Algorithm](https://tools.ietf.org/html/rfc6238)
- [RFC 4226 - HOTP: An HMAC-Based One-Time Password Algorithm](https://tools.ietf.org/html/rfc4226)
- [Google Authenticator - Supported Algorithms](https://github.com/google/google-authenticator)
- [NIST SP 800-131A Rev 2 - Transitioning to SHA-2](https://csrc.nist.gov/publications/detail/sp/800-131a/rev-2/final)
