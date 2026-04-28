---
title: "[HIGH] No E2E Tests for PIN Authentication and Lockout Flows"
severity: HIGH
domain: testing
lens: e2e-test-gaps
labels:
  - "audit:testing/e2e-test-gaps"
---

## Summary

The PIN authentication system (Badge PIN, PW1, PW3) with brute-force protection is **completely untested end-to-end**. The `PinManager` class (`components/cdc_core/src/PinManager.cpp`, 663 lines) implements critical security logic including:

- 4-8 digit PIN validation
- 3-attempt lockout with 60-second timer
- Separate PINs for Badge/FIDO2, OpenPGP user (PW1), and OpenPGP admin (PW3)
- KDF-based PIN hashing with salts
- Persistent retry counter storage in TROPIC01 R-Memory

Only trivial smoke tests exist in `test/` directory, none covering authentication flows.

## Impact

**Security Risk:**
- PIN lockout is the primary defense against brute-force attacks
- Without E2E verification, lockout bugs could allow unlimited PIN attempts
- Lockout timer expiration logic may have edge-case bugs (timer drift, deep sleep)

**Critical Scenarios Untested:**
1. Lockout starts after exactly 3 failures
2. Lockout duration is exactly 60 seconds (configurable via `LOCKOUT_DURATION_MS`)
3. Lockout clears automatically after timer expires
4. Retries reset on successful authentication
5. All 3 PIN types (Badge, PW1, PW3) have independent lockout states

## Evidence

**PinManager lockout logic** (`components/cdc_core/src/PinManager.cpp:600-660`):
```cpp
bool PinManager::isBadgeBlocked() const {
    // Blocked if retries exhausted AND lockout still active
    if (badgeRetries_ == 0) {
        return isLockoutActive();
    }
    return false;
}

void PinManager::startLockout() {
    lockoutStartMs_ = esp_timer_get_time() / 1000;  // Convert to ms
    lockoutActive_ = true;
    LOG_W(TAG, "Lockout started for %lu ms", LOCKOUT_DURATION_MS);
}

bool PinManager::isLockoutActive() const {
    if (!lockoutActive_) {
        return false;
    }

    uint32_t remaining = getLockoutRemainingMs();
    if (remaining == 0) {
        // Lockout expired - reset retries
        const_cast<PinManager*>(this)->lockoutActive_ = false;
        const_cast<PinManager*>(this)->badgeRetries_ = MAX_RETRIES;
        const_cast<PinManager*>(this)->saveToStorage();
        LOG_I(TAG, "Lockout expired, retries reset");
        return false;
    }
    return true;
}
```

**Verification flow** (`components/cdc_core/src/PinManager.cpp:299-328`):
```cpp
bool PinManager::verifyBadgePin(const char* pin) {
    if (!pin) return false;
    if (!pinLoaded_) init();

    // Check if blocked
    if (isBadgeBlocked()) {
        LOG_W(TAG, "Badge PIN blocked");
        return false;
    }

    uint8_t inputHash[BADGE_HASH_SIZE];
    if (!computeBadgeHash(pin, inputHash)) return false;

    if (compareHash(badgeHash_, inputHash, BADGE_HASH_SIZE)) {
        resetBadgeRetries();
        lockoutActive_ = false;  // Clear lockout on success
        LOG_I(TAG, "Badge PIN verified");
        return true;
    }

    badgeRetries_--;
    saveToStorage();  // Persist retry count
    LOG_W(TAG, "Wrong badge PIN, %d retries left", badgeRetries_);

    // Start lockout timer when retries exhausted
    if (badgeRetries_ == 0) {
        startLockout();
    }
    return false;
}
```

**No E2E tests exist** - only 3 vCard smoke tests in `test/` directory.

## Recommended Fix

Create E2E test file `test/e2e/e2e_pin_auth.cpp` with these test cases:

```cpp
#include <unity.h>
#include "cdc_core/PinManager.h"
#include "esp_timer.h"

using namespace cdc::core;

void setUp() {
    // Reset to default state before each test
    PinManager::instance().init();
}

void test_correct_pin_resets_retries() {
    // Start with default PIN
    TEST_ASSERT_TRUE(PinManager::instance().verifyBadgePin("123456"));
    TEST_ASSERT_EQUAL(3, PinManager::instance().getBadgeRetries());
}

void test_wrong_pin_decrements_retries() {
    PinManager::instance().verifyBadgePin("654321");
    TEST_ASSERT_EQUAL(2, PinManager::instance().getBadgeRetries());
    
    PinManager::instance().verifyBadgePin("654321");
    TEST_ASSERT_EQUAL(1, PinManager::instance().getBadgeRetries());
    
    PinManager::instance().verifyBadgePin("654321");
    TEST_ASSERT_EQUAL(0, PinManager::instance().getBadgeRetries());
}

void test_third_failure_starts_lockout() {
    PinManager::instance().verifyBadgePin("654321");
    PinManager::instance().verifyBadgePin("654321");
    PinManager::instance().verifyBadgePin("654321");
    
    TEST_ASSERT_TRUE(PinManager::instance().isBadgeBlocked());
}

void test_lockout_blocks_subsequent_attempts() {
    // Trigger lockout
    for (int i = 0; i < 3; i++) {
        PinManager::instance().verifyBadgePin("654321");
    }
    
    TEST_ASSERT_TRUE(PinManager::instance().isBadgeBlocked());
    TEST_ASSERT_FALSE(PinManager::instance().verifyBadgePin("123456"));
}

void test_successful_auth_clears_lockout() {
    // Trigger lockout
    for (int i = 0; i < 3; i++) {
        PinManager::instance().verifyBadgePin("654321");
    }
    TEST_ASSERT_TRUE(PinManager::instance().isBadgeBlocked());
    
    // Correct PIN should clear lockout
    TEST_ASSERT_TRUE(PinManager::instance().verifyBadgePin("123456"));
    TEST_ASSERT_FALSE(PinManager::instance().isBadgeBlocked());
    TEST_ASSERT_EQUAL(3, PinManager::instance().getBadgeRetries());
}

void test_lockout_expires_after_duration() {
    // Trigger lockout
    for (int i = 0; i < 3; i++) {
        PinManager::instance().verifyBadgePin("654321");
    }
    
    TEST_ASSERT_TRUE(PinManager::instance().isBadgeBlocked());
    
    // Simulate time passing (for E2E, use actual sleep or mock timer)
    // In real E2E: vTaskDelay(pdMS_TO_TICKS(LOCKOUT_DURATION_MS + 1000));
    
    TEST_ASSERT_FALSE(PinManager::instance().isBadgeBlocked());
}

void test_pw1_independent_from_badge_pin() {
    // Wrong badge PIN
    PinManager::instance().verifyBadgePin("654321");
    
    // PW1 should still have full retries
    TEST_ASSERT_EQUAL(3, PinManager::instance().getPW1Retries());
    TEST_ASSERT_TRUE(PinManager::instance().verifyPW1("123456"));
}

void test_pw3_independent_from_pw1() {
    // Wrong PW1
    PinManager::instance().verifyPW1("wrong");
    
    // PW3 should still have full retries
    TEST_ASSERT_EQUAL(3, PinManager::instance().getPW3Retries());
}

extern "C" void app_main() {
    UNITY_BEGIN();
    RUN_TEST(test_correct_pin_resets_retries);
    RUN_TEST(test_wrong_pin_decrements_retries);
    RUN_TEST(test_third_failure_starts_lockout);
    RUN_TEST(test_lockout_blocks_subsequent_attempts);
    RUN_TEST(test_successful_auth_clears_lockout);
    RUN_TEST(test_lockout_expires_after_duration);
    RUN_TEST(test_pw1_independent_from_badge_pin);
    RUN_TEST(test_pw3_independent_from_pw1);
    UNITY_END();
}
```

**Add getter methods to `PinManager.h` if missing:**
```cpp
uint8_t getBadgeRetries() const { return badgeRetries_; }
uint8_t getPW1Retries() const { return pw1Retries_; }
uint8_t getPW3Retries() const { return pw3Retries_; }
```

## References

- [PinManager Implementation](components/cdc_core/src/PinManager.cpp) - Lines 290-330 (verify), 600-660 (lockout)
- [PinManager Header](components/cdc_core/include/cdc_core/PinManager.h) - Public API
- [Feature Flags](components/cdc_core/include/cdc_core/feature_flags.h) - `DEBUG_MODE` disables lockouts
- [UI Flows](docs/UI_FLOWS.md) - PIN entry screen (lines 26-37, 260-278)
