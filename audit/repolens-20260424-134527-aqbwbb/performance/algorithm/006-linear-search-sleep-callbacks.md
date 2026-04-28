---
title: "[MEDIUM] Linear search in SleepController callback management"
severity: MEDIUM
domain: cdc_hal
lens: algorithm-efficiency
labels:
  - "audit:performance/algorithm"
---

## Summary
In `components/cdc_hal/src/SleepController.cpp` (lines 317-344), the `unregisterCallbacks()` function uses linear search to find and remove callbacks by module name. The function iterates through both pre-sleep and wakeup callback arrays.

**Evidence:**
```cpp
void Esp32SleepController::unregisterCallbacks(const char* moduleName) {
    // Remove from pre-sleep callbacks
    for (size_t i = 0; i < preSleepCount_; ) {
        if (preSleepCallbacks_[i].moduleName &&
            strcmp(preSleepCallbacks_[i].moduleName, moduleName) == 0) {
            // Shift remaining entries - O(n) additional operation
            for (size_t j = i; j < preSleepCount_ - 1; j++) {
                preSleepCallbacks_[j] = preSleepCallbacks_[j + 1];
            }
            preSleepCount_--;
        } else {
            i++;
        }
    }

    // Remove from wakeup callbacks - same pattern
    for (size_t i = 0; i < wakeupCount_; ) {
        if (wakeupCallbacks_[i].moduleName &&
            strcmp(wakeupCallbacks_[i].moduleName, moduleName) == 0) {
            for (size_t j = i; j < wakeupCount_ - 1; j++) {
                wakeupCallbacks_[j] = wakeupCallbacks_[j + 1];
            }
            wakeupCount_--;
        } else {
            i++;
        }
    }
}
```

## Impact
- **Array shifting**: Each removal requires shifting remaining elements, O(n) memory operations.
- **String comparisons**: `strcmp()` called for each element during search.
- **Total cost**: O(n × k) per unregister, where n is number of callbacks and k is average module name length.

## Evidence
**File**: `components/cdc_hal/src/SleepController.cpp`
**Lines**: 317-344
**Context**: Linear search with array shifting for both pre-sleep and wakeup callback arrays.

## Recommended Fix
Use a hash table indexed by module name for O(1) lookup and removal:

```cpp
// In Esp32SleepController.h
#define CALLBACK_HASH_SIZE 16

struct CallbackEntry {
    SleepCallback callback;
    const char* moduleName;
    uint32_t hash;
};

CallbackEntry preSleepHash_[CALLBACK_HASH_SIZE];
CallbackEntry wakeupHash_[CALLBACK_HASH_SIZE];
uint8_t preSleepCount_ = 0;
uint8_t wakeupCount_ = 0;

// Hash function
static inline uint32_t hashModuleName(const char* name) {
    uint32_t h = 5381;
    while (*name) {
        h = ((h << 5) + h) + *name++;
    }
    return h;
}

// O(1) unregister
void Esp32SleepController::unregisterCallbacks(const char* moduleName) {
    uint32_t hash = hashModuleName(moduleName);
    
    // Remove from pre-sleep
    for (int i = 0; i < CALLBACK_HASH_SIZE; i++) {
        uint32_t idx = (hash + i) % CALLBACK_HASH_SIZE;
        if (!preSleepHash_[idx].moduleName) break;
        if (strcmp(preSleepHash_[idx].moduleName, moduleName) == 0) {
            preSleepHash_[idx].moduleName = nullptr;
            preSleepCount_--;
            break;
        }
    }
    
    // Remove from wakeup (same pattern)
    // ...
}
```

Alternatively, if callbacks are registered/unregistered infrequently, document the current implementation as acceptable.

## References
- [Hash table for fast lookups](https://en.wikipedia.org/wiki/Hash_table)
