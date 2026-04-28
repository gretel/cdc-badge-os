---
title: "[HIGH] Array bounds edge case: offset+3 could overflow HMAC buffer in TOTP generation"
severity: HIGH
domain: totp
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `components/mod_totp/src/TotpStore.cpp:416-421`, the TOTP generation code computes an offset from the last HMAC byte and then reads 4 bytes starting from that offset. The offset is calculated as `hmac[hmacLen - 1] & 0x0F`, which gives a value 0-15. For HMAC-SHA1 (20 bytes), HMAC-SHA256 (32 bytes), and HMAC-SHA512 (64 bytes), the code accesses `hmac[offset]` through `hmac[offset + 3]`. With offset up to 15 and SHA1's 20-byte digest, this is safe. However, the code doesn't validate that `hmacLen >= offset + 4`, which could theoretically cause a bounds issue if the HMAC function returns an unexpectedly small digest.

## Impact
- **Memory Safety**: If `hmacCompute` returns a digest smaller than 4 bytes (edge case), the code would read past the valid digest area.
- **CORRECTness**: The maximum offset (15) + 4 bytes = 19, which is within SHA1's 20-byte digest, but this is a tight margin with no explicit validation.
- **Edge Case**: A malformed HMAC implementation or future change to the algorithm could break this assumption.

## Evidence
File: `components/mod_totp/src/TotpStore.cpp:410-423`

```cpp
uint8_t hmac[64] = {};
size_t hmacLen = 0;
if (!hmacCompute(algorithm, secret, secretLen, counterBytes, 8, hmac, &hmacLen)) {
    return 0;
}

int offset = hmac[hmacLen - 1] & 0x0F;  // offset = 0..15
uint32_t binary =
    ((hmac[offset] & 0x7F) << 24) |     // reads hmac[offset..offset+3]
    ((hmac[offset + 1] & 0xFF) << 16) |
    ((hmac[offset + 2] & 0xFF) << 8) |
    (hmac[offset + 3] & 0xFF);

return binary % POWERS_10[digits];
```

The code assumes `hmacLen >= offset + 4` but never validates this. For SHA1 (20 bytes), max access is `hmac[19]` which is valid. But if `hmacLen` were somehow 4 or less, `hmac[hmacLen - 1]` would still work but `hmac[offset + 3]` could overflow.

## Recommended Fix
Add explicit bounds validation before the packed read:

```cpp
int offset = hmac[hmacLen - 1] & 0x0F;
// Ensure we have enough bytes for the 4-byte packed read
if (hmacLen < 4 || offset + 3 >= static_cast<int>(hmacLen)) {
    return 0;  // Invalid HMAC length
}

uint32_t binary =
    ((hmac[offset] & 0x7F) << 24) |
    ((hmac[offset + 1] & 0xFF) << 16) |
    ((hmac[offset + 2] & 0xFF) << 8) |
    (hmac[offset + 3] & 0xFF);
```

Alternatively, define minimum digest sizes as constants and validate:

```cpp
static constexpr size_t MIN_HMAC_LEN = 20;  // SHA1 is smallest
if (hmacLen < MIN_HMAC_LEN) {
    return 0;
}
int offset = hmac[hmacLen - 1] & 0x0F;
// offset is 0-15, so offset+3 is 3-18, which is < 20, always safe
```

## References
- [RFC 6238 TOTP](https://datatracker.ietf.org/doc/html/rfc6238#section-4) - Specifies SHA-1 as minimum
- [HMAC-Digest truncation](https://en.wikipedia.org/wiki/HMAC#Example) - Standard says 4-byte truncation
- FIDO2 spec requires at least SHA-1 (20 bytes) for HMAC-SHA1
