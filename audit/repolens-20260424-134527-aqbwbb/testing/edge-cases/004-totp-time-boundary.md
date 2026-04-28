---
title: "[MEDIUM] TOTP generateCode() edge cases for time boundary conditions"
severity: MEDIUM
domain: mod_totp
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary
The `TotpStore::generateCode()` function at `TotpStore.cpp:482-511` has edge cases around time handling that lack test coverage:

1. **Epoch time (1970)** - `time_t` value of 0 or near epoch
2. **Year 2038 overflow** - Near 32-bit time_t limit (2147483647)
3. **DST transition** - Time changes during DST boundaries
4. **Leap second** - Time during leap second insertion
5. **Very old time** - `tm_year < 124` (before 2024) should be invalid
6. **Period edge** - `period = 0` handled but not tested
7. **Counter overflow** - When `timestamp / period` exceeds uint64_t

**Evidence** (file:line):
- `TotpStore.cpp:388-428` - `generate()` function with time calculations
- `TotpStore.cpp:516-522` - `isTimeValid()` checks `tm_year >= 124`
- `TotpStore.cpp:398-400` - Period = 0 fallback

```cpp
// isTimeValid at TotpStore.cpp:516-522
bool TotpStore::isTimeValid() const {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_year >= 124; // 2024+
}
```

The check uses `tm_year` which is years since 1900, but edge cases around year boundaries aren't tested.

## Impact
- **Time-sensitive bugs**: Year 2038 bug could cause silent failures
- **Leap year handling**: February 29 edge cases not tested
- **Timezone issues**: DST transitions could cause duplicate or missing codes

## Evidence
No edge case tests exist for time boundaries. Current test usage:
```cpp
// TotpModule.cpp:295-307 - Only tests happy path
int8_t remaining = TotpStore::instance().generateCode(slot, code);
if (remaining < 0) {
    cdc::serial::Console::printf("ERROR: time not valid\r\n");
    return;
}
```

Missing test cases:
- `time_t = 0` (Jan 1, 1970)
- `time_t = 2147483647` (Jan 19, 2038 - 32-bit overflow)
- `period = 0` - should use default
- `digits = 0` or `digits > 8` - should use default
- `timestamp` near leap second

## Recommended Fix
Add edge case tests for time boundaries:

```cpp
void test_totp_epoch_time() {
    TotpStore store;
    uint32_t code = store.generate(secret, 20, 0, 30, 6, TotpAlgorithm::SHA1);
    // Should generate valid code for epoch
    TEST_ASSERT_NOT_EQUAL(0, code);
}

void test_totp_year_2038_boundary() {
    TotpStore store;
    time_t near_overflow = 2147483647; // Jan 19, 2038
    uint32_t code = store.generate(secret, 20, near_overflow, 30, 6, TotpAlgorithm::SHA1);
    TEST_ASSERT_NOT_EQUAL(0, code);
}

void test_totp_period_zero() {
    TotpStore store;
    time_t now = time(nullptr);
    uint32_t code = store.generate(secret, 20, now, 0, 6, TotpAlgorithm::SHA1);
    // Should use default period of 30
    TEST_ASSERT_NOT_EQUAL(0, code);
}

void test_totp_digits_boundary() {
    TotpStore store;
    time_t now = time(nullptr);
    // Test digits = 0 (should use default 6)
    uint32_t code = store.generate(secret, 20, now, 30, 0, TotpAlgorithm::SHA1);
    TEST_ASSERT_NOT_EQUAL(0, code);
    
    // Test digits = 9 (should use default 6)
    code = store.generate(secret, 20, now, 30, 9, TotpAlgorithm::SHA1);
    TEST_ASSERT_NOT_EQUAL(0, code);
}

void test_totp_pre_2024_time() {
    TotpStore store;
    time_t old_time = 1609459200; // Jan 1, 2021
    bool valid = store.isTimeValid();
    // Depends on actual system time, but should handle gracefully
}

void test_totp_secret_boundary() {
    TotpStore store;
    time_t now = time(nullptr);
    // Empty secret
    uint32_t code = store.generate(nullptr, 0, now, 30, 6, TotpAlgorithm::SHA1);
    TEST_ASSERT_EQUAL(0, code);
    
    // Secret too long
    uint8_t long_secret[33];
    memset(long_secret, 0x42, 33);
    code = store.generate(long_secret, 33, now, 30, 6, TotpAlgorithm::SHA1);
    TEST_ASSERT_EQUAL(0, code);
}
```

## References
- RFC 6238 TOTP specification
- Year 2038 problem (Y2K38)
- POSIX time_t specification
