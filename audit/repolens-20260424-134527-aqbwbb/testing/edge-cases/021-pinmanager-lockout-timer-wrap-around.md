---
title: "[MEDIUM] PinManager lockout timer edge case: unsigned wrap-around"
severity: MEDIUM
domain: security/timing
lens: edge-cases
labels:
  - "audit:testing/edge-cases"
---

## Summary

The `PinManager::getLockoutRemainingMs()` function (`components/cdc_core/src/PinManager.cpp:628-640`) calculates elapsed time using unsigned 32-bit arithmetic. When `nowMs` is less than `lockoutStartMs_` due to timer wrap-around or clock adjustments, the subtraction produces a large positive number due to unsigned arithmetic, potentially causing incorrect lockout behavior.

**Location**: `components/cdc_core/src/PinManager.cpp:633-634`

The code at line 634 performs `nowMs - lockoutStartMs_` where both are `uint32_t`. If `nowMs < lockoutStartMs_` (e.g., due to timer wrap-around at ~49.7 days, or system reset with NVS-persisted state), the result wraps to a very large value (~4.3 billion), which correctly triggers the `elapsed >= LOCKOUT_DURATION_MS` check. However, this behavior is **implicit** and could be made more explicit for clarity.

Additionally, if the device is powered off with an active lockout and powered back on after the lockout period has elapsed, the timer comparison might behave unexpectedly depending on how `lockoutStartMs_` is persisted.

## Impact

**Security Consistency Risk**: 
1. **Timer wrap-around**: After ~49.7 days (32-bit millisecond counter wraps), the elapsed calculation produces a large value. While this technically results in correct behavior (lockout expires), the logic is implicit and could be confusing.

2. **Power-off edge case**: If `lockoutStartMs_` is persisted to NVS (it's not currently, but could be in future changes), a power-off/power-on cycle could cause incorrect lockout timing.

3. **Maintenance burden**: The implicit wrap-around handling relies on unsigned arithmetic properties, which is not obvious to future maintainers.

## Evidence

**Code at line 618-622** (`startLockout()`):
```cpp
void PinManager::startLockout() {
    lockoutStartMs_ = esp_timer_get_time() / 1000;  // Convert to ms
    lockoutActive_ = true;
    LOG_W(TAG, "Lockout started for %lu ms", LOCKOUT_DURATION_MS);
}
```

**Code at line 628-640** (`getLockoutRemainingMs()`):
```cpp
uint32_t PinManager::getLockoutRemainingMs() const {
    if (!lockoutActive_ || badgeRetries_ > 0) {
        return 0;
    }

    uint32_t nowMs = esp_timer_get_time() / 1000;
    uint32_t elapsed = nowMs - lockoutStartMs_;  // Potential wrap-around here

    if (elapsed >= LOCKOUT_DURATION_MS) {
        return 0;
    }
    return LOCKOUT_DURATION_MS - elapsed;
}
```

**Code at line 646-661** (`isLockoutActive()`):
```cpp
bool PinManager::isLockoutActive() const {
    if (!lockoutActive_) {
        return false;
    }

    uint32_t remaining = getLockoutRemainingMs();
    if (remaining == 0) {
        // Lockout expired - reset retries (const_cast needed for lazy update)
        const_cast<PinManager*>(this)->lockoutActive_ = false;
        const_cast<PinManager*>(this)->badgeRetries_ = MAX_RETRIES;
        const_cast<PinManager*>(this)->saveToStorage();
        LOG_I(TAG, "Lockout expired, retries reset");
        return false;
    }
    return true;
}
```

**Test case that exposes edge case**:
1. Set `lockoutStartMs_ = 0xFFFFFFFF` (near wrap-around)
2. Call `getLockoutRemainingMs()` with `nowMs = 0x00001000` (after wrap)
3. `elapsed = 0x00001000 - 0xFFFFFFFF = 0x00001001` (wraps correctly, but implicit)

**Current behavior**: Works correctly due to unsigned wrap-around properties
**Desired behavior**: Explicit handling for clarity and maintainability

## Recommended Fix

Add explicit wrap-around handling to make the logic clear and robust:

```cpp
uint32_t PinManager::getLockoutRemainingMs() const {
    if (!lockoutActive_ || badgeRetries_ > 0) {
        return 0;
    }

    uint32_t nowMs = esp_timer_get_time() / 1000;
    
    // Handle timer wrap-around explicitly
    uint32_t elapsed;
    if (nowMs >= lockoutStartMs_) {
        elapsed = nowMs - lockoutStartMs_;
    } else {
        // Timer wrapped around (e.g., after ~49.7 days)
        elapsed = UINT32_MAX - lockoutStartMs_ + nowMs;
    }

    if (elapsed >= LOCKOUT_DURATION_MS) {
        return 0;
    }
    return LOCKOUT_DURATION_MS - elapsed;
}
```

**Alternative**: Use a safer monotonic time source that handles wrap-around internally, or document the implicit behavior:

```cpp
uint32_t PinManager::getLockoutRemainingMs() const {
    if (!lockoutActive_ || badgeRetries_ > 0) {
        return 0;
    }

    uint32_t nowMs = esp_timer_get_time() / 1000;
    uint32_t elapsed = nowMs - lockoutStartMs_;  // Safe: unsigned wrap-around handles timer rollover

    if (elapsed >= LOCKOUT_DURATION_MS) {
        return 0;
    }
    return LOCKOUT_DURATION_MS - elapsed;
}
```

## References

- [ESP32 Timer Wrap-around](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_timer.html)
- [Unsigned Integer Wrap-around Behavior](https://en.cppreference.com/w/cpp/language/integer_conversion)
- [Timing Side-Channel Best Practices](https://cheatsheetseries.owasp.org/cheatsheets/Timing_Side_Channel_Cheat_Sheet.html)
