---
title: "[MEDIUM] AttestationKeyService Key Management Lacks Unit Test Coverage"
severity: MEDIUM
domain: Testing
lens: unit-test-gaps
labels:
  - "audit:testing/unit-test-gaps"
---

## Summary
The `AttestationKeyService` class (`components/cdc_core/src/AttestationKeyService.cpp`, 172 lines) manages the attestation key lifecycle with no unit tests. Critical untested functions include:

- `init()` (line 19) - Service initialization
- `start()` (line 31) - Service start
- `stop()` (line 42) - Service stop
- `onTick()` (line 50) - Periodic key verification
- `ensureKey()` (line 102) - Key generation and verification
- `loadStoredHash()` (line 66) - NVS hash load
- `saveStoredHash()` (line 84) - NVS hash save

## Impact
**Attestation Risk:** AttestationKeyService provides device identity:
1. `ensureKey()` has 8+ conditional branches - untested edge cases
2. Key generation on empty slot is unproven
3. Key regeneration on wrong curve is untested
4. Hash mismatch detection and regeneration is untested
5. NVS save/load error paths are unproven
6. Session management (isSessionActive → sessionStart) is untested

## Evidence
File: `components/cdc_core/src/AttestationKeyService.cpp`

Line 102-170: `ensureKey()` - Complex key management
```cpp
bool AttestationKeyService::ensureKey() {
    if (!secureElement_) {
        return false;  // Path 1
    }
    if (!secureElement_->isSessionActive()) {
        if (!secureElement_->sessionStart()) {
            return false;  // Path 2
        }
    }

    uint8_t pubkey[64] = {};
    hal::SeResult res = secureElement_->eccGetPublicKey(ATTESTATION_ECC_SLOT, pubkey, &curve);

    if (res == hal::SeResult::SLOT_EMPTY) {
        if (secureElement_->eccGenerate(ATTESTATION_ECC_SLOT, hal::EccCurve::P256) != hal::SeResult::OK) {
            return false;  // Path 3
        }
        res = secureElement_->eccGetPublicKey(ATTESTATION_ECC_SLOT, pubkey, &curve);
    }

    if (res != hal::SeResult::OK) {
        return false;  // Path 4
    }

    if (curve != hal::EccCurve::P256) {
        secureElement_->eccDelete(ATTESTATION_ECC_SLOT);
        if (secureElement_->eccGenerate(ATTESTATION_ECC_SLOT, hal::EccCurve::P256) != hal::SeResult::OK) {
            return false;  // Path 5
        }
        res = secureElement_->eccGetPublicKey(ATTESTATION_ECC_SLOT, pubkey, &curve);
        if (res != hal::SeResult::OK) return false;  // Path 6
    }

    uint8_t hash[32] = {};
    mbedtls_sha256(pubkey, sizeof(pubkey), hash, 0);

    uint8_t stored[32] = {};
    if (loadStoredHash(stored, sizeof(stored))) {
        if (memcmp(stored, hash, sizeof(hash)) == 0) {
            return true;  // Path 7 - Hash matches
        }
        // Hash mismatch - regenerate
        secureElement_->eccDelete(ATTESTATION_ECC_SLOT);
        if (secureElement_->eccGenerate(ATTESTATION_ECC_SLOT, hal::EccCurve::P256) != hal::SeResult::OK) {
            return false;  // Path 8
        }
        res = secureElement_->eccGetPublicKey(ATTESTATION_ECC_SLOT, pubkey, &curve);
        if (res != hal::SeResult::OK) return false;
        mbedtls_sha256(pubkey, sizeof(pubkey), hash, 0);
    }

    if (!saveStoredHash(hash, sizeof(hash))) {
        LOG_W(TAG, "Failed to store attestation key hash");  // Path 9
    }

    return true;
}
```

Line 50-58: `onTick()` - Periodic check
```cpp
void AttestationKeyService::onTick(uint32_t nowMs) {
    if (state_ != ServiceState::STARTED || ready_) return;  // Early exit
    if (nowMs - lastAttemptMs_ < RETRY_INTERVAL_MS) return;  // Rate limit
    lastAttemptMs_ = nowMs;
    if (ensureKey()) {
        ready_ = true;
        LOG_I(TAG, "Attestation key ready");
    }
}
```

Current test coverage:
```bash
$ find test/ -name "*.cpp" -exec grep -l "AttestationKeyService" {} \;
# Returns nothing - no AttestationKeyService tests exist
```

## Recommended Fix
Create `test/test_attestation_key/test_attestation_key.cpp` with test cases:

1. **Lifecycle tests:**
   - Test `init()` transitions UNINITIALIZED → INITIALIZED
   - Test `start()` transitions INITIALIZED → STARTED
   - Test `stop()` transitions STARTED → STOPPED
   - Test `init()` idempotent (already initialized)

2. **Key management tests:**
   - Test `ensureKey()` with empty slot generates key
   - Test `ensureKey()` with valid key returns true
   - Test `ensureKey()` with wrong curve regenerates
   - Test `ensureKey()` with hash mismatch regenerates

3. **Rate limit tests:**
   - Test `onTick()` respects RETRY_INTERVAL_MS
   - Test `onTick()` sets ready_ on success

4. **Error handling tests:**
   - Test `ensureKey()` with null secureElement returns false
   - Test `ensureKey()` with session failure returns false
   - Test `ensureKey()` with generate failure returns false

Example test:
```cpp
void test_ensureKey_empty_slot_generates() {
    auto& svc = AttestationKeyService::instance();
    auto* se = new MockSecureElement();
    se->setEccSlotEmpty(ATTESTATION_ECC_SLOT);
    svc.setSecureElement(se);
    svc.start();
    
    bool result = svc.ensureKey();
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(se->eccGenerateCalled);
}

void test_ensureKey_wrong_curve_regenerates() {
    auto& svc = AttestationKeyService::instance();
    auto* se = new MockSecureElement();
    se->setEccSlot(ATTESTATION_ECC_SLOT, EccCurve::K256);  // Wrong curve
    svc.setSecureElement(se);
    svc.start();
    
    bool result = svc.ensureKey();
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, se->eccDeleteCallCount);  // Delete wrong curve
    TEST_ASSERT_TRUE(se->eccGenerateCalled);  // Generate P256
}

void test_ensureKey_hash_mismatch_regenerates() {
    auto& svc = AttestationKeyService::instance();
    auto* se = new MockSecureElement();
    se->setEccSlot(ATTESTATION_ECC_SLOT, EccCurve::P256);
    se->setStoredHashMismatch();  // Stored hash differs
    svc.setSecureElement(se);
    svc.start();
    
    bool result = svc.ensureKey();
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, se->eccDeleteCallCount);  // Delete mismatched
    TEST_ASSERT_TRUE(se->eccGenerateCalled);  // Generate new
}
```

## References
- File: `components/cdc_core/include/cdc_core/AttestationKeyService.h` - Full API
- File: `components/cdc_hal/include/cdc_hal/ISecureElement.h` - Secure element interface
