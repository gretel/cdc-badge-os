---
title: "[LOW] snprintf buffer size mismatch: codeOut buffer declared as 9 bytes but snprintf uses 7-9 bytes"
severity: LOW
domain: totp
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `components/mod_totp/src/TotpStore.cpp:498-502`, the `generateCode` function uses `snprintf` to format TOTP codes with different digit counts (6, 7, or 8 digits). The buffer sizes used in `snprintf` are 9, 8, and 7 bytes respectively, but these don't account for the null terminator properly when the code variable is exactly the right size. For example, `snprintf(codeOut, 7, "%06lu", code)` writes 6 digits + 1 null = 7 bytes, which fits exactly. However, if `codeOut` is declared as `char codeOut[9]` (line 494), the 7-byte format leaves 2 bytes unused, and the 8-byte format leaves 1 byte unused.

## Impact
- **Buffer Waste**: The buffer sizes are slightly inconsistent - using a uniform 9-byte buffer for all cases would be cleaner.
- **Edge Case**: If `code` is very large (e.g., due to a bug), the formatting could truncate unexpectedly.
- **Consistency**: Using different buffer sizes for different digit counts makes the code harder to maintain.

## Evidence
File: `components/mod_totp/src/TotpStore.cpp:494-503`

```cpp
int8_t TotpStore::generateCode(uint16_t slot, char* codeOut) {
    if (!codeOut) return -1;

    TotpAccount account = {};
    if (!readAccount(slot, &account)) {
        return -1;
    }

    // ...

    uint32_t code = generate(account.secret, account.secretLen, time(nullptr),
                              account.period, account.digits,
                              static_cast<TotpAlgorithm>(account.algorithm));

    if (account.digits == 8) {
        snprintf(codeOut, 9, "%08lu", static_cast<unsigned long>(code));  // 8 digits + null = 9
    } else if (account.digits == 7) {
        snprintf(codeOut, 8, "%07lu", static_cast<unsigned long>(code));  // 7 digits + null = 8
    } else {
        snprintf(codeOut, 7, "%06lu", static_cast<unsigned long>(code));  // 6 digits + null = 7
    }

    return static_cast<int8_t>(timeRemaining(account.period));
}
```

The caller in `TotpModule.cpp:301` declares:
```cpp
char code[9] = {};
```

This is correct for the maximum case (8 digits + null), but the `snprintf` calls use varying sizes (7, 8, 9) which is slightly inconsistent.

## Recommended Fix
Use a uniform buffer size for all cases to simplify the code:

```cpp
// Always use 9 bytes (max digits + null)
snprintf(codeOut, 9, "%0*u", static_cast<int>(account.digits), static_cast<unsigned long>(code));
```

Or keep the existing logic but use consistent buffer size:
```cpp
if (account.digits == 8) {
    snprintf(codeOut, 9, "%08lu", static_cast<unsigned long>(code));
} else if (account.digits == 7) {
    snprintf(codeOut, 9, "%07lu", static_cast<unsigned long>(code));  // Use 9 for consistency
} else {
    snprintf(codeOut, 9, "%06lu", static_cast<unsigned long>(code));  // Use 9 for consistency
}
```

Or use a single format string:
```cpp
char format[16];
snprintf(format, sizeof(format), "%%0%dlu", account.digits);
snprintf(codeOut, 9, format, static_cast<unsigned long>(code));
```

## References
- [snprintf safety](https://en.wikipedia.org/wiki/Printf_format_string#Variations) - Buffer sizes must include null terminator
- [C string termination](https://en.cppreference.com/w/cpp/string/byte/snprintf) - Always need space for '\0'
