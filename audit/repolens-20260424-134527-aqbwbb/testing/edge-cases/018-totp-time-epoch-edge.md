---
title: "[MEDIUM] TOTP generation uses system time directly without validation for epoch edge cases"
severity: MEDIUM
domain: totp
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `components/mod_totp/src/TotpStore.cpp:493`, the `generateCode` function calls `time(nullptr)` directly to get the current timestamp for TOTP calculation. The `isTimeValid()` function at line 520-525 checks if the year is >= 2024, but this doesn't cover all edge cases:
1. **Year 2038 problem**: `time_t` on ESP32 is typically a 32-bit signed integer, which overflows on Jan 19, 2038 (Unix epoch overflow).
2. **Negative timestamps**: If the system clock is before 1970, `time(nullptr)` returns a negative value, which could cause issues in TOTP calculation.
3. **Clock rollback**: If the clock jumps backward (e.g., after a battery reset), the TOTP counter could go backward, potentially causing unexpected behavior.

## Impact
- **2038 Overflow**: After Jan 19, 2038, `time_t` will overflow and return negative values, breaking TOTP generation.
- **Negative Timestamps**: If `time(nullptr)` returns a negative value, the counter calculation `timestamp / period` could produce unexpected results.
- **Edge Case**: The code doesn't handle the case where `time(nullptr)` returns `(time_t)-1` (error condition).

## Evidence
File: `components/mod_totp/src/TotpStore.cpp:493-525`

```cpp
uint32_t code = generate(account.secret, account.secretLen, time(nullptr),
                          account.period, account.digits,
                          static_cast<TotpAlgorithm>(account.algorithm));
```

And the `isTimeValid()` function:
```cpp
bool TotpStore::isTimeValid() const {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_year >= 124; // 2024+
}
```

The `generate` function at line 388-424:
```cpp
uint64_t counter = static_cast<uint64_t>(timestamp / period);
```

If `timestamp` is negative (before 1970 or after 2038), the division could produce unexpected results.

## Recommended Fix
Add explicit timestamp validation and handle edge cases:

```cpp
bool TotpStore::isTimeValid() const {
    time_t now = time(nullptr);
    
    // Check for error return
    if (now == (time_t)-1) {
        return false;
    }
    
    // Check for reasonable range (2024 to 2038)
    // 1704067200 = Jan 1, 2024 00:00:00 UTC
    // 2147483647 = Jan 19, 2038 03:14:07 UTC (max 32-bit signed)
    if (now < 1704067200 || now > 2147483647) {
        return false;
    }
    
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_year >= 124; // 2024+
}

uint32_t TotpStore::generate(const uint8_t* secret, size_t secretLen, time_t timestamp,
                             uint32_t period, uint8_t digits, TotpAlgorithm algorithm) const {
    // Validate timestamp range
    if (timestamp < 0 || timestamp > 2147483647) {
        return 0;
    }
    
    // ... rest of function
}
```

Alternatively, use `uint32_t` for the timestamp parameter to avoid signed overflow issues:

```cpp
uint32_t TotpStore::generate(const uint8_t* secret, size_t secretLen, uint32_t timestamp,
                             uint32_t period, uint8_t digits, TotpAlgorithm algorithm) const {
    // timestamp is now unsigned, no negative issues
    uint64_t counter = static_cast<uint64_t>(timestamp / period);
    // ...
}
```

## References
- [Year 2038 problem](https://en.wikipedia.org/wiki/Year_2038_problem) - 32-bit time_t overflow
- [Unix epoch](https://en.wikipedia.org/wiki/Unix_time) - Time calculation reference
- [ESP32 time handling](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/time.html) - ESP32 time functions
