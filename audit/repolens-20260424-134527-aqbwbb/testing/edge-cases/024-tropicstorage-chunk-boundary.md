---
title: "[MEDIUM] TropicStorage::forEachSlot chunk boundary overflow"
severity: MEDIUM
domain: cdc_core/TropicStorage
lens: edge-case-testing
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `TropicStorage::forEachSlot` (file: `components/cdc_core/src/TropicStorage.cpp:87-123`), when iterating over slots with `fromSlot` and `toSlot` parameters, the calculation of `endChunk` can cause an out-of-bounds array access when `toSlot` is at the maximum value (0xFFFF).

The problematic code at line 107:
```cpp
uint16_t endChunk = toSlot / CHUNK_SLOTS;
```

With `CHUNK_SLOTS = 64` and `toSlot = 0xFFFF`, `endChunk = 1023`. The loop then iterates up to chunk 1023, but if the actual R-MEM space is smaller, this can iterate beyond valid chunks.

## Impact
- **Memory safety**: Can read beyond allocated NVS chunks if slot range exceeds actual hardware capacity
- **Performance**: Unnecessary iterations over non-existent chunks
- **Correctness**: May process slots outside module's allowed range

## Evidence
File: `components/cdc_core/src/TropicStorage.cpp`, lines 87-123

```cpp
bool TropicStorage::forEachSlot(uint8_t moduleId, uint16_t fromSlot, uint16_t toSlot,
                                SlotCallback cb, void* ctx) {
    // ...
    uint16_t startChunk = fromSlot / CHUNK_SLOTS;  // Line 106
    uint16_t endChunk = toSlot / CHUNK_SLOTS;       // Line 107

    CacheEntry entries[CHUNK_SLOTS] = {};
    for (uint16_t chunk = startChunk; chunk <= endChunk; chunk++) {  // Line 110
        if (!loadChunk(chunk, entries)) {
            return false;
        }
        // ...
    }
}
```

The `loadChunk` function at line 368 handles missing chunks gracefully by returning `true` with zeroed entries, but the loop still iterates unnecessarily. More critically, if `toSlot` is passed as `0xFFFF` (common sentinel for "max"), it will iterate over 1024 chunks.

## Recommended Fix
Add bounds checking to limit chunk iteration to the actual R-MEM capacity:

```cpp
bool TropicStorage::forEachSlot(uint8_t moduleId, uint16_t fromSlot, uint16_t toSlot,
                                SlotCallback cb, void* ctx) {
    if (!cb) return false;
    if (fromSlot > toSlot) return false;
    
    auto& slotMap = TropicSlotMap::instance();
    TropicSlotMap::SlotRange range = {};
    if (!slotMap.getRangeByModuleId(moduleId, TropicSlotMap::SlotType::RMEM, &range)) {
        return false;
    }
    if (fromSlot == 0 || toSlot == 0xFFFF) {
        fromSlot = range.start;
        toSlot = range.end;
    }
    if (fromSlot < range.start) fromSlot = range.start;
    if (toSlot > range.end) toSlot = range.end;

    // ADD: Clamp chunk range to actual capacity
    uint16_t maxChunk = (slotMap.rmemMax() / CHUNK_SLOTS);
    uint16_t startChunk = fromSlot / CHUNK_SLOTS;
    uint16_t endChunk = toSlot / CHUNK_SLOTS;
    if (endChunk > maxChunk) endChunk = maxChunk;  // Prevent overflow

    CacheEntry entries[CHUNK_SLOTS] = {};
    for (uint16_t chunk = startChunk; chunk <= endChunk; chunk++) {
        // ...
    }
}
```

## References
- C++ Core Guidelines, [F.19: For an iteration, use a for loop or a while loop](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#F19)
- CWE-190: Integer overflow or wraparound
