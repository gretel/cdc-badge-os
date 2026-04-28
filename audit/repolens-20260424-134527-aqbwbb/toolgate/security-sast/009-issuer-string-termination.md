---
title: "[LOW] Potential non-null-terminated issuer string in TOTP payload"
severity: LOW
domain: security-sast
lens: toolgate
labels:
  - "CWE-170: Improper Null Termination"
---

## Summary
In `TotpStore::addAccount()` and `TotpStore::updateAccount()`, the `issuer` field is copied using `strncpy()` without explicit null-termination. While the struct is zero-initialized, this pattern is error-prone and could lead to non-null-terminated strings if the code changes.

**Locations:**
- `components/mod_totp/src/TotpStore.cpp:267` (addAccount)
- `components/mod_totp/src/TotpStore.cpp:327` (updateAccount)
- `components/mod_totp/src/TotpStore.cpp:178` (readAccount)

## Impact
- **Subtle bugs**: If the struct initialization changes or `strncpy` behavior varies, the issuer string may not be null-terminated
- **Print functions**: `printf()` or `print()` functions expecting null-terminated strings could read past buffer
- **Consistency**: Inconsistent with other string handling in the codebase

## Evidence

### Current code (TotpStore.cpp:267)
```cpp
TotpPayload payload = {};
if (issuer) {
    strncpy(payload.issuer, issuer, sizeof(payload.issuer) - 1);
}
```

The `strncpy()` writes at most 31 chars (for 32-byte buffer), leaving byte 32 as null from the `{}` initialization. However, this relies on:
1. The struct being zero-initialized
2. No subsequent code overwriting the last byte

### Similar pattern in readAccount (TotpStore.cpp:178)
```cpp
strncpy(out->issuer, payload.issuer, sizeof(out->issuer) - 1);
```

## Recommended Fix
Explicitly null-terminate after `strncpy()`:

```cpp
TotpPayload payload = {};
if (issuer) {
    strncpy(payload.issuer, issuer, sizeof(payload.issuer) - 1);
    payload.issuer[sizeof(payload.issuer) - 1] = '\0';  // Explicit null-terminate
}
```

Or use `strlcpy()` if available:
```cpp
strlcpy(payload.issuer, issuer, sizeof(payload.issuer));
```

## References
- CWE-170: Improper Null Termination - https://cwe.mitre.org/data/definitions/170.html
- `strncpy()` pitfalls - https://www.gnu.org/software/libc/manual/html_node/Truncated-Strings.html
