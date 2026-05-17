---
title: "[009] [MEDIUM] Duplicate callback registration patterns in SleepController"
severity: MEDIUM
domain: code-readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
In `components/cdc_hal/src/SleepController.cpp`, the `registerPreSleepCallback` (lines 248-272) and `registerWakeupCallback` (lines 276-300) functions have nearly identical implementations:

```cpp
// Lines 248-272
bool Esp32SleepController::registerPreSleepCallback(const SleepCallbackEntry& entry) {
    if (preSleepCount_ >= MAX_CALLBACKS) {
        LOG_W(TAG, "Pre-sleep callback limit reached");
        return false;
    }

    // Insert sorted by priority (lower priority = earlier in array)
    size_t insertPos = preSleepCount_;
    for (size_t i = 0; i < preSleepCount_; i++) {
        if (entry.priority < preSleepCallbacks_[i].priority) {
            insertPos = i;
            break;
        }
    }

    // Shift existing entries
    for (size_t i = preSleepCount_; i > insertPos; i--) {
        preSleepCallbacks_[i] = preSleepCallbacks_[i - 1];
    }

    preSleepCallbacks_[insertPos] = entry;
    preSleepCount_++;
    // ... logging
    return true;
}

// Lines 276-300 - Nearly identical!
bool Esp32SleepController::registerWakeupCallback(const SleepCallbackEntry& entry) {
    if (wakeupCount_ >= MAX_CALLBACKS) {
        LOG_W(TAG, "Wakeup callback limit reached");
        return false;
    }

    // Insert sorted by priority
    size_t insertPos = wakeupCount_;
    for (size_t i = 0; i < wakeupCount_; i++) {
        if (entry.priority < wakeupCallbacks_[i].priority) {
            insertPos = i;
            break;
        }
    }

    // Shift existing entries
    for (size_t i = wakeupCount_; i > insertPos; i--) {
        wakeupCallbacks_[i] = wakeupCallbacks_[i - 1];
    }

    wakeupCallbacks_[insertPos] = entry;
    wakeupCount_++;
    // ... logging
    return true;
}
```

## Impact
- **Maintenance burden**: Changes to the insertion logic must be applied to both functions
- **Inconsistency risk**: Bug fix in one function might be forgotten in the other
- **Code bloat**: ~45 lines of nearly identical code

## Evidence
**File**: `components/cdc_hal/src/SleepController.cpp:248-300`

The two functions differ only in:
- Variable names (`preSleepCallbacks_` vs `wakeupCallbacks_`)
- Count variables (`preSleepCount_` vs `wakeupCount_`)
- Log messages

## Recommended Fix
Extract a template or private helper method:

```cpp
template <typename ArrayType>
bool Esp32SleepController::registerCallback(ArrayType* callbacks, size_t* count,
                                            const SleepCallbackEntry& entry,
                                            const char* logPrefix) {
    if (*count >= MAX_CALLBACKS) {
        LOG_W(TAG, "%s callback limit reached", logPrefix);
        return false;
    }

    size_t insertPos = *count;
    for (size_t i = 0; i < *count; i++) {
        if (entry.priority < callbacks[i].priority) {
            insertPos = i;
            break;
        }
    }

    for (size_t i = *count; i > insertPos; i--) {
        callbacks[i] = callbacks[i - 1];
    }

    callbacks[insertPos] = entry;
    (*count)++;

    LOG_I(TAG, "Registered %s callback: %s (priority %d)",
          logPrefix, entry.moduleName, entry.priority);
    return true;
}

// Usage
bool Esp32SleepController::registerPreSleepCallback(const SleepCallbackEntry& entry) {
    return registerCallback(preSleepCallbacks_, &preSleepCount_, entry, "pre-sleep");
}

bool Esp32SleepController::registerWakeupCallback(const SleepCallbackEntry& entry) {
    return registerCallback(wakeupCallbacks_, &wakeupCount_, entry, "wakeup");
}
```

## References
- [DRY Principle - Don't Repeat Yourself](https://en.wikipedia.org/wiki/Don%27t_repeat_yourself)
- [C++ Core Guidelines - D.12: Use templates for generic code](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#d12-use-templates-for-generic-code)
