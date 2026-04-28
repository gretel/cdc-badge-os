---
title: "[MEDIUM] Linear search in TropicSlotMap validation with nested comparison"
severity: MEDIUM
domain: cdc_core
lens: algorithm-efficiency
labels:
  - "audit:performance/algorithm"
---

## Summary
In `components/cdc_core/src/TropicSlotMap.cpp` (lines 64-131), the `validateOnce()` function uses nested loops to check for overlaps and duplicate module/type combinations. For each entry, it compares against all subsequent entries, resulting in O(n²) complexity.

**Evidence:**
```cpp
for (size_t i = 0; i < kSlotMapCount; i++) {
    // ... bounds checking ...
    
    for (size_t j = i + 1; j < kSlotMapCount; j++) {
        const auto& b = kSlotMap[j];
        if (a.type == b.type) {
            bool overlap = !(a.end < b.start || b.end < a.start);
            if (overlap) {
                setError("slot map overlap detected");
                return;
            }
        }
        if (strcmp(a.moduleName, b.moduleName) == 0 && a.type == b.type) {
            // duplicate check
        }
    }
}
```

## Impact
- **Startup latency**: Slot map validation runs once at initialization. With ~20-30 entries, this is ~400-900 comparisons.
- **String comparisons**: Each comparison involves `strcmp()` which is O(k) where k is the string length.
- **Total cost**: O(n² × k) where n is number of entries and k is average module name length.

## Evidence
**File**: `components/cdc_core/src/TropicSlotMap.cpp`
**Lines**: 64-131
**Context**: The nested loop structure compares each slot map entry against all subsequent entries for overlap detection and duplicate checking.

## Recommended Fix
Since this is a one-time validation at startup and the number of entries is small (20-30), the current implementation is acceptable. However, for clarity and maintainability:

1. **Add a comment** explaining why O(n²) is acceptable here (small, fixed n; runs once at startup).
2. **Consider pre-sorting** the slot map by type and start position, which would allow early termination of the inner loop when ranges no longer overlap.

For a more scalable solution:
```cpp
// Sort entries by type and start position first
// Then only compare entries of the same type
// Early exit inner loop when a.start > b.end
```

## References
- [Algorithm complexity for small n](https://en.wikipedia.org/wiki/Time_complexity)
