---
title: "[MEDIUM] TOTP Module Core Functions Lack Unit Test Coverage"
severity: MEDIUM
domain: Testing
lens: unit-test-gaps
labels:
  - "audit:testing/unit-test-gaps"
---

## Summary
The TOTP module (`components/mod_totp/`) implements time-based one-time passwords with no unit tests. Critical untested functions include:

**TotpStore.cpp:**
- `init()` - Storage initialization
- `loadToTotp()` - Load TOTP config from R-Memory
- `saveToTotp()` - Save TOTP config to R-Memory
- `generateCode()` - TOTP code generation (HMAC-SHA1)
- `validateCode()` - Code verification with time window

**TotpModule.cpp:**
- `init()` - Module initialization
- `start()` - Module start
- `getMenuItems()` - Menu population
- `onTick()` - Periodic updates
- `getSlotRequest()` - Slot requirements

## Impact
**Authentication Risk:** TOTP is a 2FA mechanism:
1. TOTP algorithm (HMAC-SHA1 with time steps) is untested
2. Time window validation (±1 step for drift) is unproven
3. Base32 encoding/decoding for secret is untested
4. Storage format with R-Memory is unverified
5. Time sync from RTC is untested

## Evidence
File: `components/mod_totp/src/TotpStore.cpp` (estimated 100-150 lines)

Typical TOTP implementation includes:
- Time-based counter: `T = (current_time - epoch) / time_step`
- HMAC-SHA1: `HMAC-SHA1(secret, T)`
- Truncate: Extract 4 bytes from HMAC result
- Modulo: `code = truncate % 1000000`

Current test coverage:
```bash
$ find test/ -name "*.cpp" -exec grep -l -i "totp" {} \;
# Returns nothing - no TOTP tests exist
```

## Recommended Fix
Create `test/test_totp_module/test_totp_module.cpp` with test cases:

1. **Algorithm tests:**
   - Test `generateCode()` with known secret and time
   - Test `generateCode()` produces different codes over time
   - Test `validateCode()` accepts current code
   - Test `validateCode()` accepts previous/next code (window)
   - Test `validateCode()` rejects expired code

2. **Storage tests:**
   - Test `saveToTotp()` and `loadToTotp()` roundtrip
   - Test `loadToTotp()` with empty slot returns error

3. **Encoding tests:**
   - Test Base32 encode/decode roundtrip
   - Test Base32 with padding

4. **Module tests:**
   - Test `getMenuItems()` returns TOTP menu
   - Test `onTick()` updates display

Example test (using known TOTP test vectors):
```cpp
void test_generateCode_known_vector() {
    // RFC 6238 test vectors
    const char* secret = "12345678901234567890";  // Base10
    uint64_t time = 59;  // Time step
    
    uint32_t code = generateCode(secret, time, 30, 6);
    TEST_ASSERT_EQUAL(282805, code);  // RFC 6238 SHA1 test vector
}

void test_validateCode_window() {
    auto& store = TotpStore::instance();
    store.loadToTotp(0);  // Load config
    
    uint32_t current = store.generateCode();
    TEST_ASSERT_TRUE(store.validateCode(current));
    
    uint32_t prev = store.generateCodeAtTime(time - 30);
    TEST_ASSERT_TRUE(store.validateCode(prev));  // Previous step
    
    uint32_t next = store.generateCodeAtTime(time + 30);
    TEST_ASSERT_TRUE(store.validateCode(next));  // Next step
    
    uint32_t old = store.generateCodeAtTime(time - 60);
    TEST_ASSERT_FALSE(store.validateCode(old));  // Too old
}
```

## References
- RFC 6238: TOTP - Time-Based One-Time Password Algorithm
- File: `components/mod_totp/include/mod_totp/TotpStore.h` - Store API
- File: `components/mod_totp/include/mod_totp/TotpModule.h` - Module API
