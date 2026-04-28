---
title: "[MEDIUM] Hardcoded uncompressed public key format prefix (0x04)"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
The OpenPGP implementation uses hardcoded value `0x04` for uncompressed public key format prefix throughout `components/mod_gpg/src/openpgp/openpgp.cpp`. This is the SEC 1 standard prefix but appears without a named constant.

**Locations:** Lines 1379, 1587, 1590

## Impact
- **Clarity**: Value `0x04` doesn't immediately convey "uncompressed point format".
- **Maintainability**: If compressed formats are added, developers must understand all 0x04 occurrences.
- **Standards compliance**: Reference to SEC 1 standard would help documentation.

## Evidence

**Lines 1378-1382 (validation):**
```cpp
// Verify uncompressed format
if (p[0] != 0x04) {
    ESP_LOGW(TAG, "Expected uncompressed public key (0x04 prefix)");
    return apdu_sw(resp, SW_WRONG_DATA);
}
```

**Lines 1581, 1587, 1590 (format conversion):**
```cpp
// TROPIC01 returns 64 bytes (X || Y), we need to add 0x04 prefix
...
if (pubkey[0] == 0x04) {
    // Already has prefix
} else {
    pubkey_with_prefix[0] = 0x04;
}
```

The value `0x04` is used in two contexts:
1. **Validation**: Checking incoming public keys are uncompressed format
2. **Construction**: Adding prefix when TROPIC01 returns raw X||Y coordinates

## Recommended Fix

1. **Define named constants** for SEC 1 point format prefixes:
```cpp
/**
 * \brief SEC 1 elliptic curve point format prefixes
 * 
 * Reference: SEC 1 - Elliptic Curve Cryptography (https://www.secg.org/sec1-v2.pdf)
 * Section 2.3.3: Elliptic-Curve-Point-to-Octet-String Conversion
 */
namespace sec1 {
    static constexpr uint8_t POINT_COMPRESSED_XPOS = 0x02;  // Compressed, X positive
    static constexpr uint8_t POINT_COMPRESSED_XNEG = 0x03;  // Compressed, X negative
    static constexpr uint8_t POINT_UNCOMPRESSED = 0x04;     // Uncompressed (X || Y)
}
```

2. **Update usage** to use constants:
```cpp
// Before:
if (p[0] != 0x04) {
    ESP_LOGW(TAG, "Expected uncompressed public key (0x04 prefix)");
    return apdu_sw(resp, SW_WRONG_DATA);
}

// After:
if (p[0] != sec1::POINT_UNCOMPRESSED) {
    ESP_LOGW(TAG, "Expected uncompressed public key (0x04 prefix)");
    return apdu_sw(resp, SW_WRONG_DATA);
}

// Before:
if (pubkey[0] == 0x04) {
    pubkey_with_prefix[0] = 0x04;
}

// After:
if (pubkey[0] == sec1::POINT_UNCOMPRESSED) {
    pubkey_with_prefix[0] = sec1::POINT_UNCOMPRESSED;
}
```

3. **Add to existing P256 constants file** if creating the one from the P256 public key size finding.

## References
- [SEC 1 - Elliptic Curve Cryptography](https://www.secg.org/sec1-v2.pdf): Section 2.3.3
- [OpenPGP Smart Card Application 3.4.1](https://github.com/ANSSI-FR/openpgp-card)
- [FIDO2 CTAP2 Specification](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html)
- Related finding: `023-hardcoded-p256-public-key-size.md`
