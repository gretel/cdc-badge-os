---
title: "[MEDIUM] Missing range validation for TOTP digits and period parameters"
severity: MEDIUM
domain: input-sanitization
lens: serial-commands
labels:
  - "audit:security/input-sanitization"
---

## Summary
The `cmd_totp_add` function in `components/mod_totp/src/TotpModule.cpp` accepts user-provided `digits` and `period` parameters via the serial command `TOTP_ADD` but does not validate that these values are within reasonable ranges. The `atoi()` function is used without checking for negative values, zero, or excessively large numbers.

**Location:** `components/mod_totp/src/TotpModule.cpp:239-242`
**Location:** `components/mod_totp/src/TotpModule.cpp:145-154` (parseAlgo for reference)

## Impact
- **Digits**: Valid TOTP codes use 6-8 digits. Values outside this range could cause:
  - Display issues (too few or too many digits)
  - Compatibility problems with authenticator apps
  - Potential truncation or formatting issues
- **Period**: Standard TOTP period is 30s or 60s. Invalid values could cause:
  - Unusual code refresh rates
  - Synchronization issues with verification services
  - Edge cases in timeRemaining() calculations
- **Silent fallback**: Invalid values fall back to defaults without user notification, making debugging difficult

## Evidence
```cpp
// components/mod_totp/src/TotpModule.cpp:239-242
static void cmd_totp_add(const char* args) {
    // ...
    char digitsBuf[8] = {};
    char periodBuf[8] = {};
    // ...
    p = nextToken(p, digitsBuf, sizeof(digitsBuf));
    p = nextToken(p, periodBuf, sizeof(periodBuf));
    
    // No validation - any integer accepted
    uint8_t digits = digitsBuf[0] ? static_cast<uint8_t>(atoi(digitsBuf)) : TotpStore::DEFAULT_DIGITS;
    uint32_t period = periodBuf[0] ? static_cast<uint32_t>(atoi(periodBuf)) : TotpStore::DEFAULT_PERIOD;
    
    bool ok = TotpStore::instance().addAccount(name, issuer, secret, digits, period, algo);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

In `TotpStore::generate()` at line 388-400:
```cpp
uint32_t TotpStore::generate(const uint8_t* secret, size_t secretLen, time_t timestamp,
                             uint32_t period, uint8_t digits, TotpAlgorithm algorithm) const {
    // ...
    if (digits < 6 || digits > 8) {
        digits = DEFAULT_DIGITS;  // Silent fallback
    }
    if (period == 0) {
        period = DEFAULT_PERIOD;  // Silent fallback
    }
    // ...
}
```

The store has internal validation but the serial command doesn't provide feedback to users.

## Recommended Fix
Add validation in `cmd_totp_add()` before calling `addAccount()`:

```cpp
static void cmd_totp_add(const char* args) {
    // ... existing token parsing ...
    
    uint8_t digits = digitsBuf[0] ? static_cast<uint8_t>(atoi(digitsBuf)) : TotpStore::DEFAULT_DIGITS;
    uint32_t period = periodBuf[0] ? static_cast<uint32_t>(atoi(periodBuf)) : TotpStore::DEFAULT_PERIOD;
    
    // Validate digits (typical TOTP codes are 6-8 digits)
    if (digits < 6 || digits > 8) {
        cdc::serial::Console::printf("ERROR: Digits must be 6-8 (got %d)\r\n", digits);
        return;
    }
    
    // Validate period (common values are 30s or 60s)
    if (period == 0 || period > 3600) {  // Max 1 hour period
        cdc::serial::Console::printf("ERROR: Period must be 1-3600 seconds (got %lu)\r\n", (unsigned long)period);
        return;
    }
    
    bool ok = TotpStore::instance().addAccount(name, issuer, secret, digits, period, algo);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}
```

## References
- RFC 6238: TOTP - Time-Based One-Time Password Algorithm
- CWE-20: Improper Input Validation
- CWE-131: Incorrect Calculation of Buffer Size
- OATH-TOTP specifications define standard ranges for digits (6-8) and period (30s typical)
