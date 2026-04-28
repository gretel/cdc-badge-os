---
title: "[MEDIUM] Base32 decode lacks edge case tests for special characters and boundary inputs"
severity: MEDIUM
domain: mod_totp
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary
The `base32Decode()` function in `components/mod_totp/src/TotpStore.cpp` handles Base32 decoding but lacks edge case test coverage for:

1. **Empty string** - `base32Decode("", out, max)` should return 0 or -1
2. **All-whitespace string** - `"   \t\n"` should be handled gracefully
3. **Invalid characters** - Characters outside A-Z, a-z, 2-7 range (e.g., `@`, `1`, `8`, `9`, `-`)
4. **Mixed valid/invalid** - `"ABC@DEF"` - where invalid char appears in middle
5. **Very long padding** - `"AAAA========"` - excessive padding
6. **Case sensitivity edge** - lowercase vs uppercase handling at boundaries
7. **Single character** - `"A"` - minimum valid input
8. **Incomplete bit groups** - `"ABC"` (15 bits, not divisible by 8)

**Evidence** (file:line):
- `TotpStore.cpp:33-68` - `base32Decode()` implementation
- `TotpStore.cpp:26-31` - `base32CharValue()` helper function

```cpp
// base32CharValue at TotpStore.cpp:33-38
static int base32CharValue(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= '2' && c <= '7') return c - '2' + 26;
    return -1;  // Invalid char
}
```

The function returns -1 for invalid chars but tests don't verify this behavior.

## Impact
- **Security**: Invalid Base32 could bypass validation if error handling is inconsistent
- **User experience**: Poor error messages for common copy-paste errors (trailing spaces, dashes)
- **Silent data loss**: Incomplete bit groups might produce truncated secrets

## Evidence
No edge case tests exist for `base32Decode()`. Current usage at `TotpStore.cpp:260`:
```cpp
int secretLen = base32Decode(secretBase32, secret, SECRET_LEN);
if (secretLen <= 0) {
    LOG_E(TAG, "Invalid Base32 secret");
    return false;
}
```

Missing test coverage for:
- Empty string: `base32Decode("", buf, 32)` → should return -1 or 0
- Whitespace only: `base32Decode("   \t\n", buf, 32)` → should return 0
- Invalid chars: `base32Decode("ABC@DEF", buf, 32)` → should return -1
- Numbers outside range: `base32Decode("ABC189", buf, 32)` → should return -1
- Single char: `base32Decode("A", buf, 32)` → incomplete, returns 0 bytes
- Output buffer size 0: `base32Decode("AAAA", buf, 0)` → edge case for `outMax`

## Recommended Fix
Add edge case tests for Base32 decoding:

```cpp
void test_base32_empty_input() {
    uint8_t buf[32];
    int result = base32Decode("", buf, 32);
    TEST_ASSERT_EQUAL(0, result); // or -1 depending on design
}

void test_base32_whitespace_only() {
    uint8_t buf[32];
    int result = base32Decode("   \t\n\r", buf, 32);
    TEST_ASSERT_EQUAL(0, result);
}

void test_base32_invalid_chars() {
    uint8_t buf[32];
    int result = base32Decode("ABC@DEF", buf, 32);
    TEST_ASSERT_EQUAL(-1, result);
    
    result = base32Decode("ABC189", buf, 32);
    TEST_ASSERT_EQUAL(-1, result);
    
    result = base32Decode("ABC-DEF", buf, 32);
    TEST_ASSERT_EQUAL(-1, result);
}

void test_base32_single_char() {
    uint8_t buf[32];
    int result = base32Decode("A", buf, 32);
    TEST_ASSERT_EQUAL(0, result); // Incomplete, no full bytes
}

void test_base32_excessive_padding() {
    uint8_t buf[32];
    int result = base32Decode("AAAA========", buf, 32);
    TEST_ASSERT_EQUAL(1, result); // Should handle gracefully
}

void test_base32_output_buffer_zero() {
    int result = base32Decode("AAAA", nullptr, 0);
    TEST_ASSERT_EQUAL(-1, result);
}

void test_base32_special_chars() {
    uint8_t buf[32];
    // Test characters that might be confused
    int result = base32Decode("ABCDE23456", buf, 32);
    TEST_ASSERT_EQUAL(5, result); // 5 bytes from 8 chars
}
```

## References
- RFC 4648 Base32 specification
- Common Base32 edge cases in cryptography libraries
- mbedtls Base32 implementation for comparison
