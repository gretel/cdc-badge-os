---
title: "[MEDIUM] Missing logging for authentication decisions in PinManager"
severity: MEDIUM
domain: logging
lens: logging-coverage
labels:
  - "audit:observability/logging"
---

## Summary
The `PinManager` component (`components/cdc_core/src/PinManager.cpp`) has several authentication decision points that execute without logging, particularly for OpenPGP PW1 and PW3 PIN verification.

**Missing log entries:**

1. **PW1 verification** (lines 409-429): Only logs when blocked or success, missing log for wrong PIN:
```cpp
bool PinManager::verifyPW1(const char* pin) {
    ...
    if (pw1Retries_ == 0) {
        LOG_W(TAG, "PW1 blocked");  // Logged
        return false;
    }
    ...
    if (compareHash(pw1Hash_, inputHash, KDF_HASH_SIZE)) {
        resetPW1Retries();
        LOG_I(TAG, "PW1 verified");  // Logged
        return true;
    }
    pw1Retries_--;
    saveToStorage();  // No LOG_W for wrong PIN!
    return false;
}
```

2. **PW3 verification** (lines 511-531): Same issue - missing wrong PIN log:
```cpp
bool PinManager::verifyPW3(const char* pin) {
    ...
    if (compareHash(pw3Hash_, inputHash, KDF_HASH_SIZE)) {
        resetPW3Retries();
        LOG_I(TAG, "PW3 verified");
        return true;
    }
    pw3Retries_--;
    saveToStorage();  // No LOG_W for wrong PIN!
    return false;
}
```

3. **Badge PIN verification** (lines 301-329): Correctly logs wrong PIN (for comparison):
```cpp
bool PinManager::verifyBadgePin(const char* pin) {
    ...
    if (compareHash(badgeHash_, inputHash, BADGE_HASH_SIZE)) {
        resetBadgeRetries();
        lockoutActive_ = false;
        LOG_I(TAG, "Badge PIN verified");
        return true;
    }
    badgeRetries_--;
    saveToStorage();
    LOG_W(TAG, "Wrong badge PIN, %d retries left", badgeRetries_);  // Correctly logged
    ...
}
```

## Impact
- **Security observability**: Failed PIN attempts for OpenPGP (PW1/PW3) don't leave a log trail
- **Debug difficulty**: Hard to diagnose why authentication is failing without seeing retry counts
- **Inconsistent behavior**: Badge PIN logs wrong attempts but OpenPGP PINs don't

## Evidence
Compare `verifyBadgePin()` (line 325) which logs:
```cpp
LOG_W(TAG, "Wrong badge PIN, %d retries left", badgeRetries_);
```

With `verifyPW1()` (line 426) which does NOT log:
```cpp
pw1Retries_--;
saveToStorage();  // No log!
return false;
```

## Recommended Fix
Add logging to PW1 and PW3 verification failures:

1. **In `verifyPW1()` (around line 426)**:
```cpp
pw1Retries_--;
saveToStorage();  // Persist retry count
LOG_W(TAG, "Wrong PW1, %d retries left", pw1Retries_);
return false;
```

2. **In `verifyPW3()` (around line 528)**:
```cpp
pw3Retries_--;
saveToStorage();
LOG_W(TAG, "Wrong PW3, %d retries left", pw3Retries_);
return false;
```

This brings PW1/PW3 logging in line with the existing badge PIN logging pattern.

## References
- `components/cdc_core/src/PinManager.cpp` - PIN management logic
- `components/cdc_log/include/cdc_log.h` - Logging macros
