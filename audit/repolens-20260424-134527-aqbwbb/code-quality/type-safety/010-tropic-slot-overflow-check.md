---
title: "[MEDIUM] Missing bounds check in TROPIC slot iteration"
severity: MEDIUM
domain: cdc_core
lens: type-safety
labels:
  - "audit:code-quality/type-safety"
---

## Summary
The `TropicStorage::forEachSlot` function uses a callback pattern with `void* ctx` context parameter, but there's no validation that the slot index passed to the callback is within valid bounds.

## Evidence
In `components/cdc_core/include/cdc_core/TropicStorage.h` line 20-21:
```cpp
using SlotCallback = void(*)(uint16_t slot, const CacheEntry& entry, void* ctx);
using RebuildLogFn = void(*)(uint16_t slot, const char* message, void* ctx);
```

In `components/cdc_core/src/TropicStorage.cpp` line 147-156:
```cpp
bool TropicStorage::getSlot(uint8_t moduleId, uint16_t index, SlotCallback cb, void* ctx) {
    uint32_t slot = static_cast<uint32_t>(start) + index;
    CacheEntry entry;
    
    if (!getEntry(static_cast<uint16_t>(slot), &entry)) return false;
    
    if (!isEntryAllowed(static_cast<uint16_t>(slot), entry.moduleId)) return false;
    
    cb(static_cast<uint16_t>(slot), entry, ctx);  // No bounds check on slot
    return true;
}
```

The `slot` value is cast from `uint32_t` to `uint16_t` without checking for overflow.

## Impact
1. **Silent truncation**: If `start + index` exceeds `UINT16_MAX`, the slot value will be silently truncated.
2. **Wrong slot access**: The truncated value may point to an unexpected slot.

## Recommended Fix
Add explicit bounds checking before the callback:

```cpp
bool TropicStorage::getSlot(uint8_t moduleId, uint16_t index, SlotCallback cb, void* ctx) {
    uint32_t slot = static_cast<uint32_t>(start) + index;
    
    // Check for overflow before casting
    if (slot > UINT16_MAX) {
        LOG_E(TAG, "Slot index overflow: start=%u, index=%u", start, index);
        return false;
    }
    
    CacheEntry entry;
    if (!getEntry(static_cast<uint16_t>(slot), &entry)) return false;
    
    if (!isEntryAllowed(static_cast<uint16_t>(slot), entry.moduleId)) return false;
    
    cb(static_cast<uint16_t>(slot), entry, ctx);
    return true;
}
```

## References
- C++ Core Guidelines F.46: "Use 'const' and 'constexpr' to help avoid unintended conversions and reassignments"
- C++ Core Guidelines I.1: "Use integers for counting, not for bit patterns"

</content>