---
title: "[MEDIUM] TOTP_ADD command lacks validation for digits, period, and algorithm ranges"
severity: MEDIUM
domain: api-design/request-validation
lens: serial-command-validation
labels:
  - "audit:api-design/request-validation"
---

## Summary
The `TOTP_ADD` serial command in `components/mod_totp/src/TotpModule.cpp:250-252` parses optional parameters (digits, period, algorithm) using `atoi()` without validating that values fall within reasonable ranges. Invalid values are silently accepted or default to store values.

**File**: `components/mod_totp/src/TotpModule.cpp`  
**Lines**: 250-252  
**Function**: `cmd_totp_add()`

## Impact
- **Invalid digits**: Values outside 6-8 range accepted (standard TOTP uses 6 or 8 digits)
- **Invalid period**: Zero or very large periods accepted (standard is 30 seconds)
- **Invalid algorithm**: Any value accepted (only 0=SHA1, 1=SHA256, 2=SHA512 are valid)
- **Silent degradation**: Invalid values may cause TOTP codes to not work with standard authenticators

## Evidence
From `components/mod_totp/src/TotpModule.cpp:248-255`:

```cpp
p = nextToken(p, digitsBuf, sizeof(digitsBuf));
p = nextToken(p, periodBuf, sizeof(periodBuf));
p = nextToken(p, algoBuf, sizeof(algoBuf));

uint8_t digits = digitsBuf[0] ? static_cast<uint8_t>(atoi(digitsBuf)) : TotpStore::DEFAULT_DIGITS;
uint32_t period = periodBuf[0] ? static_cast<uint32_t>(atoi(periodBuf)) : TotpStore::DEFAULT_PERIOD;
uint8_t algo = parseAlgo(algoBuf);
```

From `components/mod_totp/include/mod_totp/TotpStore.h:28-32`:
```cpp
static constexpr uint8_t NAME_LEN = 16;
static constexpr uint8_t ISSUER_LEN = 32;
static constexpr uint8_t SECRET_LEN = 32;
static constexpr uint8_t DEFAULT_DIGITS = 6;
static constexpr uint32_t DEFAULT_PERIOD = 30;
```

From `components/mod_totp/src/TotpModule.cpp:136-148` (parseAlgo):
```cpp
static uint8_t parseAlgo(const char* token) {
    if (!token || !*token) return static_cast<uint8_t>(TotpAlgorithm::SHA1);
    // ... string parsing ...
    return static_cast<uint8_t>(atoi(buf));  // No range validation!
}
```

Problematic inputs:
- `TOTP_ADD account secret 10 30 sha1` → digits=10 (unusual)
- `TOTP_ADD account secret 6 0 sha1` → period=0 (division by zero risk in code!)
- `TOTP_ADD account secret 6 30 99` → algo=99 (invalid algorithm)
- `TOTP_ADD account secret 6 30 sha99` → algo=0 (defaults to SHA1 silently)

## Recommended Fix
Add range validation for each parameter:

```cpp
uint8_t digits = digitsBuf[0] ? static_cast<uint8_t>(atoi(digitsBuf)) : TotpStore::DEFAULT_DIGITS;
uint32_t period = periodBuf[0] ? static_cast<uint32_t>(atoi(periodBuf)) : TotpStore::DEFAULT_PERIOD;
uint8_t algo = parseAlgo(algoBuf);

// Validate digits (typically 6 or 8, allow 5-10 range)
if (digits < 5 || digits > 10) {
    cdc::serial::Console::printf("ERROR: Digits must be 5-10 (default 6)\r\n");
    return;
}

// Validate period (avoid zero, reasonable range 10-300 seconds)
if (period == 0 || period > 300) {
    cdc::serial::Console::printf("ERROR: Period must be 10-300 seconds (default 30)\r\n");
    return;
}

// Validate algorithm (0=SHA1, 1=SHA256, 2=SHA512)
if (algo > 2) {
    cdc::serial::Console::printf("ERROR: Algorithm must be 0 (SHA1), 1 (SHA256), or 2 (SHA512)\r\n");
    return;
}
```

For `parseAlgo()`, add explicit range check:
```cpp
static uint8_t parseAlgo(const char* token) {
    if (!token || !*token) return static_cast<uint8_t>(TotpAlgorithm::SHA1);
    char buf[8] = {};
    // ... existing string parsing ...
    uint8_t val = static_cast<uint8_t>(atoi(buf));
    if (val <= 2) return val;
    return static_cast<uint8_t>(TotpAlgorithm::SHA1);  // default on invalid
}
```

## References
- RFC 6238 (TOTP): https://datatracker.ietf.org/doc/html/rfc6238
- RFC 4226 (HOTP): https://datatracker.ietf.org/doc/html/rfc4226
- Google Authenticator uses 6 digits, 30-second period
