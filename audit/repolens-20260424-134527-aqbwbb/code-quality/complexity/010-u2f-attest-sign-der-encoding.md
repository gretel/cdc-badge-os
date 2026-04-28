---
title: "[LOW] Complex DER encoding logic in u2f_attest_sign() function"
severity: LOW
domain: mod_fido2/u2f
lens: cyclomatic-complexity
labels:
  - "audit:code-quality/complexity"
---

## Summary

The `u2f_attest_sign()` function in `components/mod_fido2/src/u2f.cpp` (lines 45-120) has complex DER encoding logic that manually constructs X.509-style ECDSA signatures. The function has multiple conditional branches for handling leading zeros and MSB padding.

**Estimated Cyclomatic Complexity: ~9**

## Impact

**Readability:**
- DER encoding logic is dense and hard to follow
- Multiple manual calculations for padding and lengths
- Error handling is scattered across the function

**Maintainability:**
- Adding support for different signature formats requires understanding all edge cases
- Bug fixes in padding logic may affect other parts

## Evidence

**File:** `components/mod_fido2/src/u2f.cpp:45-120`

**Code excerpt:**
```cpp
static bool u2f_attest_sign(const uint8_t *data, size_t data_len,
                             uint8_t *signature, uint8_t *sig_len) {
    auto* se = get_se();
    if (!se) {  // +1
        return false;
    }

    uint8_t hash[32];
    sha256(data, data_len, hash);

    uint8_t raw_sig[64];  // R || S (each 32 bytes)
    size_t raw_len = sizeof(raw_sig);
    if (se->ecdsaSign(U2F_ATTEST_SLOT, hash, sizeof(hash), raw_sig, &raw_len) !=
            cdc::hal::SeResult::OK ||  // +1
        raw_len != sizeof(raw_sig)) {  // +1
        LOG_E("U2F", "Attestation signing failed");
        return false;
    }

    // Convert raw R||S to DER format
    uint8_t *p = signature;
    *p++ = DER_SEQUENCE_TAG;

    // Find first non-zero byte in R (skip leading zeros)
    int r_start = 0;
    while (r_start < RAW_SIGNATURE_COMPONENT_SIZE - 1 && raw_sig[r_start] == 0) r_start++;  // +1 (loop)
    int r_len = RAW_SIGNATURE_COMPONENT_SIZE - r_start;
    // Add padding byte if MSB is set
    int r_pad = (raw_sig[r_start] & DER_INTEGER_NEGATIVE_MASK) ? 1 : 0;  // +1 (ternary)

    // Find first non-zero byte in S
    int s_start = 0;
    while (s_start < RAW_SIGNATURE_COMPONENT_SIZE - 1 && raw_sig[RAW_SIGNATURE_COMPONENT_SIZE + s_start] == 0) s_start++;  // +1 (loop)
    int s_len = RAW_SIGNATURE_COMPONENT_SIZE - s_start;
    int s_pad = (raw_sig[RAW_SIGNATURE_COMPONENT_SIZE + s_start] & DER_INTEGER_NEGATIVE_MASK) ? 1 : 0;  // +1 (ternary)

    // Calculate total length
    int total_len = 2 + r_pad + r_len + 2 + s_pad + s_len;
    *p++ = total_len;

    // R integer
    *p++ = DER_INTEGER_TAG;
    *p++ = r_pad + r_len;
    if (r_pad) {  // +1
        *p++ = 0x00;
    }
    memcpy(p, raw_sig + r_start, r_len);
    p += r_len;

    // S integer
    *p++ = DER_INTEGER_TAG;
    *p++ = s_pad + s_len;
    if (s_pad) {  // +1
        *p++ = 0x00;
    }
    memcpy(p, raw_sig + RAW_SIGNATURE_COMPONENT_SIZE + s_start, s_len);
    p += s_len;

    *sig_len = p - signature;
    return true;
}
```

**Branching count:**
- Line 51: `if (!se)` (+1)
- Line 60: `if (se->ecdsaSign(...) != OK || raw_len != sizeof(raw_sig))` (+2)
- Line 70: `while (r_start < ... && raw_sig[r_start] == 0)` (+1)
- Line 72: `r_pad = (...) ? 1 : 0` (+1)
- Line 76: `while (s_start < ... && raw_sig[...] == 0)` (+1)
- Line 78: `s_pad = (...) ? 1 : 0` (+1)
- Line 88: `if (r_pad)` (+1)
- Line 95: `if (s_pad)` (+1)

Total: ~10 independent paths

## Recommended Fix

**Extract DER encoding into helper functions:**

```cpp
// Helper to encode a single DER integer
static uint8_t* encode_der_integer(uint8_t* p, const uint8_t* data, size_t len) {
    // Find first non-zero byte
    int start = 0;
    while (start < len - 1 && data[start] == 0) start++;
    int actual_len = len - start;

    // Check if MSB is set (needs padding)
    int pad = (data[start] & 0x80) ? 1 : 0;

    *p++ = DER_INTEGER_TAG;
    *p++ = pad + actual_len;
    if (pad) *p++ = 0x00;
    memcpy(p, data + start, actual_len);
    p += actual_len;

    return p;
}

// Helper to encode R and S components
static uint8_t* encode_ecdsa_der(uint8_t* p, const uint8_t* raw_sig) {
    p = encode_der_integer(p, raw_sig, 32);       // R
    p = encode_der_integer(p, raw_sig + 32, 32);  // S
    return p;
}

static bool u2f_attest_sign(const uint8_t *data, size_t data_len,
                             uint8_t *signature, uint8_t *sig_len) {
    auto* se = get_se();
    if (!se) return false;

    uint8_t hash[32];
    sha256(data, data_len, hash);

    uint8_t raw_sig[64];
    size_t raw_len = sizeof(raw_sig);
    if (se->ecdsaSign(U2F_ATTEST_SLOT, hash, sizeof(hash), raw_sig, &raw_len) !=
            cdc::hal::SeResult::OK || raw_len != sizeof(raw_sig)) {
        LOG_E("U2F", "Attestation signing failed");
        return false;
    }

    // Build DER SEQUENCE
    uint8_t* p = signature;
    *p++ = DER_SEQUENCE_TAG;

    // Calculate length first
    uint8_t temp[128];
    uint8_t* end = encode_ecdsa_der(temp, raw_sig);
    *p++ = end - temp;

    // Copy encoded integers
    memcpy(p, temp, end - temp);
    p += end - temp;

    *sig_len = p - signature;
    return true;
}
```

**Expected result:**
- Main function reduced to ~30 lines
- DER encoding logic isolated in reusable helpers
- Easier to test encoding separately from signing

## References

- [DER encoding - Wikipedia](https://en.wikipedia.org/wiki/Canonical_and_ordinal_DER_encoding)
- [ECDSA signature format](https://tools.ietf.org/html/rfc3279#section-2.2.3)
