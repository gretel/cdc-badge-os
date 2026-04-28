---
title: "[MEDIUM] Insufficient input validation with atoi() in serial command handlers"
severity: MEDIUM
domain: security-sast
lens: toolgate
labels:
  - "CWE-20: Improper Input Validation"
  - "CWE-1341: Multiple or Inconsistent Normalization"
---

## Summary
Multiple serial command handlers use `atoi()` to parse numeric arguments without proper validation. This can lead to silent parsing failures where invalid input is converted to `0` without error indication.

**Locations:**
- `components/mod_totp/src/TotpModule.cpp:147,250,251,274,293`
- `components/mod_gpg/src/GpgModule.cpp:175`
- `components/mod_password/src/PasswordModule.cpp:225,288,313,641`

## Impact
- **Silent failures**: `atoi()` returns `0` for invalid input (e.g., "abc", "123abc"), making it hard to distinguish between valid `0` and parsing errors
- **Logic bugs**: Account indices, TOTP periods, and digits can be set to unexpected values
- **User confusion**: Commands may appear to succeed when they actually used default/wrong values

## Evidence

### Example 1: TOTP algorithm parsing (TotpModule.cpp:147)
```cpp
return static_cast<uint8_t>(atoi(buf));
```
If `buf` contains "invalid", `atoi` returns `0` (SHA1), silently accepting invalid input.

### Example 2: TOTP digits/period parsing (TotpModule.cpp:250-251)
```cpp
uint8_t digits = digitsBuf[0] ? static_cast<uint8_t>(atoi(digitsBuf)) : TotpStore::DEFAULT_DIGITS;
uint32_t period = periodBuf[0] ? static_cast<uint32_t>(atoi(periodBuf)) : TotpStore::DEFAULT_PERIOD;
```
Input "100digits" would parse as `100` instead of failing.

### Example 3: Account index parsing (TotpModule.cpp:274)
```cpp
uint16_t index = static_cast<uint16_t>(atoi(args));
```
No validation that `args` contains only digits or that the result is within valid range.

## Recommended Fix
Replace `atoi()` with `strtol()` or `strtoul()` with full validation:

```cpp
// Instead of:
uint16_t index = static_cast<uint16_t>(atoi(args));

// Use:
char* endptr;
long val = strtol(args, &endptr, 10);
if (*endptr != '\0' || val < 0 || val > UINT16_MAX) {
    Console::printf("ERROR: Invalid index\r\n");
    return;
}
uint16_t index = static_cast<uint16_t>(val);
```

For the algorithm parser:
```cpp
// Instead of:
return static_cast<uint8_t>(atoi(buf));

// Use:
char* endptr;
long val = strtol(buf, &endptr, 10);
if (*endptr == '\0' && val >= 0 && val <= 2) {
    return static_cast<uint8_t>(val);
}
return static_cast<uint8_t>(TotpAlgorithm::SHA1);  // Default
```

## References
- CWE-20: Improper Input Validation - https://cwe.mitre.org/data/definitions/20.html
- C Standard Library `strtol()` - https://en.cppreference.com/w/c/string/byte/strtol
- C Standard Library `atoi()` limitations - https://en.cppreference.com/w/c/string/byte/atoi
