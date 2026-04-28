---
title: "[MEDIUM] TROPIC01 sleep() function swallows error status"
severity: MEDIUM
domain: cdc_hal
lens: error-handling
labels:
  - "audit:error-handling/error-swallowing"
---

## Summary
In `components/cdc_hal/src/Tropic01Element.cpp:252-270`, the `sleep()` function logs an error when `lt_sleep()` fails but does not return an error status or update the session state. The function always completes without signaling failure to the caller.

**Location:** `components/cdc_hal/src/Tropic01Element.cpp:260-267`

```cpp
void Tropic01Element::sleep() {
    lock();

    if (!sessionActive_) {
        unlock();
        return;
    }

    lt_ret_t ret = lt_sleep(&handle_, TR01_L2_SLEEP_KIND_SLEEP);
    if (ret != LT_OK) {
        LOG_E(TAG, "Sleep failed (%s)", lt_ret_verbose(ret));
        handleSessionError(ret);  // Only marks session inactive
    } else {
        sessionActive_ = false;
        LOG_I(TAG, "Entered sleep mode");
    }

    unlock();
    // No return value to signal failure!
}
```

## Impact
- Callers cannot detect when sleep operation fails
- System may proceed assuming device is in sleep mode when it is not
- Power management logic may behave incorrectly
- Debugging sleep-related issues becomes harder as failures are only visible in logs

## Evidence
The function signature is `void sleep()` with no return value. When `lt_sleep()` fails (line 261), the error is logged (line 262) and `handleSessionError()` is called (line 263), but execution continues and the function returns normally.

Compare with `sessionStart()` at line 196 which properly returns `bool` and returns `false` on failure.

## Recommended Fix
Change the function signature to return a boolean status:

```cpp
/**
 * \brief Requests secure-element sleep mode.
 * \return `true` on success, `false` if sleep failed.
 */
bool Tropic01Element::sleep() {
    lock();

    if (!sessionActive_) {
        unlock();
        return true;  // Already inactive
    }

    lt_ret_t ret = lt_sleep(&handle_, TR01_L2_SLEEP_KIND_SLEEP);
    if (ret != LT_OK) {
        LOG_E(TAG, "Sleep failed (%s)", lt_ret_verbose(ret));
        handleSessionError(ret);
        unlock();
        return false;  // Signal failure to caller
    }

    sessionActive_ = false;
    LOG_I(TAG, "Entered sleep mode");
    unlock();
    return true;
}
```

Then update all callers to check the return value.

## References
- C++ Error Handling Best Practices: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rclass-error
- ESP32 TROPIC01 Driver Documentation (libtropic)
