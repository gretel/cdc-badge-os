---
title: "[HIGH] PinManager Core Functions Lack Unit Test Coverage"
severity: HIGH
domain: Testing
lens: unit-test-gaps
labels:
  - "audit:testing/unit-test-gaps"
---

## Summary
The `PinManager` class (`components/cdc_core/src/PinManager.cpp`, 663 lines) is a critical security component that manages all device PINs (Badge/FIDO2, OpenPGP PW1, PW3) with no dedicated unit tests. Only a single smoke test exists in `test/test_vcard_store/test_vcard_store.cpp` that tests a completely different component.

Key untested public functions include:
- `verifyBadgePin()` (line 300) - PIN verification with retry logic
- `changeBadgePin()` (line 337) - PIN change workflow
- `setBadgePin()` (line 346) - PIN setting with validation
- `verifyPW1()` (line 411) - OpenPGP user PIN verification
- `verifyPW3()` (line 511) - OpenPGP admin PIN verification
- `computeBadgeHash()` (line 221) - SHA-256 hash computation
- `computeKdfHash()` (line 243) - OpenPGP KDF hash computation
- `isBadgeBlocked()` (line 607) - Lockout logic
- `startLockout()` (line 616) - Lockout timer
- `getLockoutRemainingMs()` (line 625) - Lockout duration calculation

## Impact
**Security Risk:** The PIN manager is the primary authentication mechanism for the device. Without unit tests:
1. Edge cases in PIN validation (min/max length, digit-only check) may have undetected bugs
2. Lockout timer logic (60-second timeout, retry reset) is unproven
3. KDF hash computation (OpenPGP spec compliance) has no verification
4. Badge hash truncation (LEFT(SHA256, 16)) is untested
5. Error paths (null pointers, invalid storage) may not work correctly

## Evidence
File: `components/cdc_core/src/PinManager.cpp`
- Line 300-328: `verifyBadgePin()` - Has 5 conditional branches (null check, loaded check, blocked check, hash match, lockout start)
- Line 243-274: `computeKdfHash()` - Complex loop for OpenPGP KDF with iterations
- Line 607-613: `isBadgeBlocked()` - Combines retries=0 AND lockout check
- Line 648-661: `isLockoutActive()` - Has lazy expiration reset logic with const_cast

Current test coverage:
```bash
$ find test/ -name "*.cpp" -exec grep -l "PinManager" {} \;
# Returns nothing - no PinManager tests exist
```

## Recommended Fix
Create `test/test_pin_manager/test_pin_manager.cpp` with test cases for:

1. **PIN validation tests:**
   - Test `verifyBadgePin()` with correct/incorrect PINs
   - Test retry countdown (3 attempts → 0)
   - Test lockout activation after 3 failures
   - Test lockout expiration after 60s

2. **PIN change tests:**
   - Test `setBadgePin()` with valid PINs (4-8 digits)
   - Test `setBadgePin()` with invalid PINs (<4, >8, non-digits)
   - Test `changeBadgePin()` with current PIN verification

3. **Hash computation tests:**
   - Test `computeBadgeHash()` produces consistent LEFT(SHA256, 16)
   - Test `computeKdfHash()` with known salt/PIN combinations

4. **Edge case tests:**
   - Test null pointer handling
   - Test default PIN loading
   - Test storage load/save roundtrip

Example test structure:
```cpp
void test_verifyBadgePin_correct() {
    auto& pm = PinManager::instance();
    pm.loadDefaults();  // Setup
    bool result = pm.verifyBadgePin("123456");
    TEST_ASSERT_TRUE(result);
}

void test_verifyBadgePin_wrong_retries() {
    auto& pm = PinManager::instance();
    pm.loadDefaults();
    pm.verifyBadgePin("000000");
    pm.verifyBadgePin("000000");
    pm.verifyBadgePin("000000");
    TEST_ASSERT_EQUAL(0, pm.getBadgeRetries());
}

void test_computeBadgeHash_consistency() {
    uint8_t hash1[16], hash2[16];
    auto& pm = PinManager::instance();
    pm.computeBadgeHash("123456", hash1);
    pm.computeBadgeHash("123456", hash2);
    TEST_ASSERT_EQUAL_MEMORY(hash1, hash2, 16);
}
```

## References
- OpenPGP Card Specification: Iterated+Salted S2K (RFC 4880)
- File: `components/cdc_core/include/cdc_core/PinManager.h` - Full API documentation
- File: `components/cdc_core/src/PinManager.cpp` - Implementation details
