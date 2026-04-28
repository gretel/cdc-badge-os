---
title: "[MEDIUM] Linear search in SleepManager inhibitor tracking"
severity: MEDIUM
domain: cdc_os_ui
lens: algorithm-efficiency
labels:
  - "audit:performance/algorithm"
---

## Summary
In `components/cdc_os_ui/src/SleepManager.cpp` (lines 203-249), the `addSleepInhibitor()` and `removeSleepInhibitor()` functions use linear search through the inhibitors array. Each call performs O(n) comparisons with `strcmp()`.

**Evidence:**
```cpp
bool SleepManager::addSleepInhibitor(const char* reason) {
    // Check if already exists
    for (uint8_t i = 0; i < inhibitorCount_; i++) {
        if (inhibitors_[i] && strcmp(inhibitors_[i], reason) == 0) {
            return false;  // Already exists
        }
    }
    // ...
}

bool SleepManager::removeSleepInhibitor(const char* reason) {
    // Find and remove
    for (uint8_t i = 0; i < inhibitorCount_; i++) {
        if (inhibitors_[i] && strcmp(inhibitors_[i], reason) == 0) {
            // Shift remaining entries - O(n) additional operation
            for (uint8_t j = i; j < inhibitorCount_ - 1; j++) {
                inhibitors_[j] = inhibitors_[j + 1];
            }
            // ...
        }
    }
}
```

## Impact
- **Repeated calls**: Sleep inhibitors may be added/removed frequently during UI interactions.
- **Array shifting**: `removeSleepInhibitor()` also shifts array elements, adding O(n) memory operations.
- **Total cost**: O(n × k) per operation, where n is number of inhibitors and k is average reason string length.

## Evidence
**File**: `components/cdc_os_ui/src/SleepManager.cpp`
**Lines**: 203-249
**Context**: Linear search with `strcmp()` for both add and remove operations.

## Recommended Fix
Use a hash-based set for O(1) average-case operations. With `MAX_SLEEP_INHIBITORS` typically small (5-10), a simple hash table or even a sorted array with binary search would work:

```cpp
// Option 1: Simple hash table
#define INHIBITOR_HASH_SIZE 16

struct InhibitorEntry {
    const char* reason;
    uint32_t hash;
};

InhibitorEntry inhibitorHash_[INHIBITOR_HASH_SIZE];

// Add with O(1) duplicate check
bool SleepManager::addSleepInhibitor(const char* reason) {
    uint32_t hash = hashString(reason);
    uint32_t idx = hash % INHIBITOR_HASH_SIZE;
    
    // Check for duplicate
    for (int i = 0; i < INHIBITOR_HASH_SIZE; i++) {
        uint32_t probe = (idx + i) % INHIBITOR_HASH_SIZE;
        if (!inhibitorHash_[probe].reason) break;  // Empty slot
        if (strcmp(inhibitorHash_[probe].reason, reason) == 0) {
            return false;  // Already exists
        }
    }
    
    // Add if space available
    // ...
}

// Remove with O(1) find
bool SleepManager::removeSleepInhibitor(const char* reason) {
    uint32_t hash = hashString(reason);
    uint32_t idx = hash % INHIBITOR_HASH_SIZE;
    
    // Find and clear
    for (int i = 0; i < INHIBITOR_HASH_SIZE; i++) {
        uint32_t probe = (idx + i) % INHIBITOR_HASH_SIZE;
        if (!inhibitorHash_[probe].reason) return false;
        if (strcmp(inhibitorHash_[probe].reason, reason) == 0) {
            inhibitorHash_[probe].reason = nullptr;
            // ...
            return true;
        }
    }
    return false;
}
```

## References
- [Hash set data structure](https://en.wikipedia.org/wiki/Hash_table#Hash_set)
