---
title: "[LOW] TOTP Base32 decoding doesn't handle lowercase consistently"
severity: LOW
domain: code-quality
lens: session-nuclei
labels:
  - audit:toolgate/session-nuclei
---

## Summary

In `components/mod_totp/src/TotpStore.cpp`, the `base32CharValue` function handles both uppercase and lowercase, but the implementation could be clearer:

```cpp
static int base32CharValue(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= '2' && c <= '7') return c - '2' + 26;
    return -1;
}
```

The function works correctly but the comment in the decode function says "Base32" which typically uses uppercase A-Z by convention.

## Impact

- **User experience**: Users might paste secrets in different cases (Google Authenticator often uses uppercase).
- **Consistency**: Some TOTP implementations only accept uppercase.
- **Documentation**: It's not clear what format the secret should be in.

## Evidence

**File**: `components/mod_totp/src/TotpStore.cpp` (lines 35-42)

```cpp
/**
 * \brief Converts one Base32 character into 5-bit value.
 * \param c Input character.
 * \return Value in 0..31, or `-1` if invalid.
 */
static int base32CharValue(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= '2' && c <= '7') return c - '2' + 26;
    return -1;
}
```

**File**: `components/mod_totp/src/TotpStore.cpp` (lines 52-69)

```cpp
static int base32Decode(const char* encoded, uint8_t* out, size_t outMax) {
    if (!encoded || !out) return -1;

    int buffer = 0;
    int bitsLeft = 0;
    size_t count = 0;

    for (const char* p = encoded; *p; ++p) {
        if (*p == '=' || *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            continue;
        }
        int value = base32CharValue(*p);
        if (value < 0) {
            return -1;
        }
        // ...
    }
    return static_cast<int>(count);
}
```

## Recommended Fix

**Option 1: Add documentation**
Clarify that both cases are accepted:

```cpp
/**
 * \brief Converts one Base32 character into 5-bit value.
 * \param c Input character (accepts A-Z, a-z, or 2-7).
 * \return Value in 0..31, or `-1` if invalid.
 */
```

**Option 2: Normalize to uppercase**
Convert input to uppercase before processing for consistency:

```cpp
static int base32Decode(const char* encoded, uint8_t* out, size_t outMax) {
    // ...
    for (const char* p = encoded; *p; ++p) {
        char c = *p;
        // Normalize to uppercase for consistency
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        // ...
    }
}
```

**Option 3: Add test case**
Add a test that verifies both uppercase and lowercase secrets decode to the same value.

## References

- RFC 4648: Base32 Encoding
- Google Authenticator: Base32 secret format
- TOTP RFC 6238
