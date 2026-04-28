---
title: "[MEDIUM] O(n^2) nested loop in TropicSlotMap validation"
severity: MEDIUM
domain: algorithm-efficiency
lens: algorithm
labels:
  - "nested-loops"
---

## Summary
The `validateOnce()` function in `components/cdc_core/src/TropicSlotMap.cpp` (lines 57-131) uses a nested loop to check for slot map overlaps and duplicates. With `kSlotMapCount` entries (currently 6 entries: 3 ECC + 3 RMEM), the validation performs O(n^2) comparisons.

**Evidence:**
```cpp
// components/cdc_core/src/TropicSlotMap.cpp:103-129
for (size_t i = 0; i < kSlotMapCount; i++) {
    const auto& a = kSlotMap[i];
    // ... validation of entry a ...
    
    for (size_t j = i + 1; j < kSlotMapCount; j++) {  // O(n^2) nested loop
        const auto& b = kSlotMap[j];
        if (!b.moduleName) continue;

        if (a.type == b.type) {
            bool overlap = !(a.end < b.start || b.end < a.start);
            if (overlap) {
                setError("slot map overlap detected");
                return;
            }
        }
        // ... more comparisons ...
    }
}
```

## Impact
- **Current scale**: With only 6 entries, performance impact is negligible (~15 comparisons)
- **Scalability concern**: If the slot map grows to 20+ entries (e.g., adding more modules), comparisons grow quadratically (190 comparisons for 20 entries)
- **Build-time only**: This runs once at startup, so the impact is limited to initialization time

## Recommended Fix
For the current scale (6 entries), the O(n^2) complexity is acceptable. However, if the slot map is expected to grow beyond 15-20 entries, consider:

1. **Sort-and-scan approach** (O(n log n)):
   - Sort entries by `(type, start)` once
   - Scan linearly to check adjacent entries for overlaps
   
2. **Bitset approach** (O(n) for fixed range):
   - For RMEM (512 slots): use a 512-bit bitmap
   - Mark slots as entries are processed
   - Check for collisions in O(1) per entry

Example implementation using sort-and-scan:
```cpp
// Create sortable copy
struct SortedEntry {
    TropicSlotMap::SlotType type;
    uint16_t start, end;
    const char* moduleName;
    uint8_t moduleId;
};
std::array<SortedEntry, kSlotMapCount> sorted;
// Copy and sort by (type, start)
// Scan for overlaps in O(n)
```

## References
- Big-O notation: https://en.wikipedia.org/wiki/Big_O_notation
- Interval overlap detection: https://stackoverflow.com/questions/325933/determine-whether-two-date-ranges-overlap
