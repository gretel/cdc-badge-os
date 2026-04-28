---
title: "[MEDIUM] Missing input validation for TOTP algorithm parameter in serial command"
severity: MEDIUM
domain: input-sanitization
lens: serial-commands
labels:
  - "audit:security/input-sanitization"
---

## Summary
The `cmd_totp_add` function in `components/mod_totp/src/TotpModule.cpp` accepts a user-provided algorithm parameter via the serial command `TOTP_ADD` but does not validate that the algorithm value is within the expected range (0-2 for SHA1, SHA256, SHA512). The `parseAlgo()` function at line 145-154 uses `atoi()` which can return any integer value, including negative numbers or values outside the valid enum range.

**Location:** `components/mod_totp/src/TotpModule.cpp:145-154` (parseAlgo function)
**Location:** `components/mod_totp/src/TotpModule.cpp:239-242` (cmd_totp_add usage)

## Impact
- **Garbage-in behavior**: Invalid algorithm values could be stored in the TOTP account configuration, potentially causing unexpected behavior during code generation
- **Enum mismatch**: The `TotpAlgorithm` enum only defines values 0, 1, 2 (SHA1, SHA256, SHA512), but `atoi()` can return any integer, leading to undefined behavior when the algorithm is used
- **Silent acceptance**: The command accepts any numeric value without feedback to the user about valid options

## Evidence
```cpp
// components/mod_totp/src/TotpModule.cpp:145-154
static uint8_t parseAlgo(const char* token) {
    if (!token || !*token) return static_cast<uint8_t>(TotpAlgorithm::SHA1);
    char buf[8] = {};
    size_t i = 0;
    for (; token[i] && i + 1 < sizeof(buf); i++) {
        buf[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(token[i])));
    }
    buf[i] = '\0';
    if (strcmp(buf, "sha1") == 0) return static_cast<uint8_t>(TotpAlgorithm::SHA1);
    if (strcmp(buf, "sha256") == 0) return static_cast<uint8_t>(TotpAlgorithm::SHA256);
    if (strcmp(buf, "sha512") == 0) return static_cast<uint8_t>(TotpAlgorithm::SHA512);
    return static_cast<uint8_t>(atoi(buf));  // <-- NO RANGE VALIDATION
}
```

At line 239-242:
```cpp
uint8_t algo = parseAlgo(algoBuf);  // Can return any value 0-255

bool ok = TotpStore::instance().addAccount(
    name,
    issuer[0] ? issuer : nullptr,
    secret,
    digits,
    period,
    algo  // Unvalidated algorithm value passed to store
);
```

## Recommended Fix
Add range validation to `parseAlgo()` to ensure the numeric algorithm value is within the valid range (0-2):

```cpp
static uint8_t parseAlgo(const char* token) {
    if (!token || !*token) return static_cast<uint8_t>(TotpAlgorithm::SHA1);
    char buf[8] = {};
    size_t i = 0;
    for (; token[i] && i + 1 < sizeof(buf); i++) {
        buf[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(token[i])));
    }
    buf[i] = '\0';
    if (strcmp(buf, "sha1") == 0) return static_cast<uint8_t>(TotpAlgorithm::SHA1);
    if (strcmp(buf, "sha256") == 0) return static_cast<uint8_t>(TotpAlgorithm::SHA256);
    if (strcmp(buf, "sha512") == 0) return static_cast<uint8_t>(TotpAlgorithm::SHA512);
    
    // Parse numeric value with range validation
    int algoVal = atoi(buf);
    if (algoVal >= 0 && algoVal <= 2) {
        return static_cast<uint8_t>(algoVal);
    }
    
    // Return default for invalid values
    return static_cast<uint8_t>(TotpAlgorithm::SHA1);
}
```

Alternatively, add validation in `cmd_totp_add()` after calling `parseAlgo()`:
```cpp
uint8_t algo = parseAlgo(algoBuf);
if (algo > 2) {
    cdc::serial::Console::printf("ERROR: Invalid algorithm (use 0-2 or sha1/sha256/sha512)\r\n");
    return;
}
```

## References
- CWE-20: Improper Input Validation
- CWE-131: Incorrect Calculation of Buffer Size (related to unchecked numeric input)
- ESP32 TOTP implementation should validate all user-supplied numeric parameters
