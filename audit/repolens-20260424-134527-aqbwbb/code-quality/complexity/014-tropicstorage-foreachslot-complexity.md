---
title: "[MEDIUM] Complex forEachSlot function with deeply nested conditions"
severity: MEDIUM
domain: code-quality
lens: cyclomatic-complexity
labels:
  - "complexity:high"
---

## Summary
The `forEachSlot` function in `components/cdc_core/src/TropicStorage.cpp:87` has moderate-to-high cyclomatic complexity with nested loops and multiple early-exit conditions. The function iterates R-Memory chunks and slots with 5+ branching conditions per iteration.

**Evidence:**
- File: `components/cdc_core/src/TropicStorage.cpp`
- Lines: 87-125 (38 lines)
- Nested loops: 2 levels (chunks → slots)
- Conditions per inner iteration: 5 (slot range check, entry used check, entry allowed check, module ID match, null callback)
- Total branching paths: ~10-12

## Impact
- **Readability**: Multiple continue statements make flow harder to follow
- **Debugging**: Hard to trace which condition caused a slot to be skipped
- **Testing**: Each condition combination needs test coverage
- **Modification risk**: Adding new conditions increases complexity further

## Evidence
```cpp
bool TropicStorage::forEachSlot(uint8_t moduleId, uint16_t fromSlot, uint16_t toSlot,
                                SlotCallback cb, void* ctx) {
    if (!cb) return false;                           // Branch 1
    if (fromSlot > toSlot) return false;             // Branch 2
    auto& slotMap = TropicSlotMap::instance();
    TropicSlotMap::SlotRange range = {};
    if (!slotMap.getRangeByModuleId(moduleId, TropicSlotMap::SlotType::RMEM, &range)) {
        return false;                                // Branch 3
    }
    // Range adjustment logic...
    
    uint16_t startChunk = fromSlot / CHUNK_SLOTS;
    uint16_t endChunk = toSlot / CHUNK_SLOTS;

    CacheEntry entries[CHUNK_SLOTS] = {};
    for (uint16_t chunk = startChunk; chunk <= endChunk; chunk++) {
        if (!loadChunk(chunk, entries)) {
            return false;                            // Branch 4
        }
        uint16_t slotBase = chunk * CHUNK_SLOTS;
        for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
            uint16_t slot = static_cast<uint16_t>(slotBase + i);
            if (slot < fromSlot || slot > toSlot) continue;   // Branch 5
            const CacheEntry& entry = entries[i];
            if (!isEntryUsed(entry)) continue;                // Branch 6
            if (!isEntryAllowed(slot, entry.moduleId)) continue; // Branch 7
            if (entry.moduleId != moduleId) continue;         // Branch 8
            cb(slot, entry, ctx);
        }
    }
    return true;
}
```

## Recommended Fix
Extract condition checks into descriptive predicates and use guard clauses:

1. **Create predicate helpers:**
   ```cpp
   bool TropicStorage::isSlotInRange(uint16_t slot, uint16_t from, uint16_t to) const {
       return slot >= from && slot <= to;
   }
   
   bool TropicStorage::isSlotEligible(uint16_t slot, const CacheEntry& entry, 
                                       uint8_t moduleId) const {
       return isEntryUsed(entry) && 
              isEntryAllowed(slot, entry.moduleId) && 
              entry.moduleId == moduleId;
   }
   ```

2. **Refactor main loop with early exits:**
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
       
       // Range adjustment
       if (fromSlot == 0 || toSlot == 0xFFFF) {
           fromSlot = range.start;
           toSlot = range.end;
       }
       fromSlot = std::max(fromSlot, range.start);
       toSlot = std::min(toSlot, range.end);

       uint16_t startChunk = fromSlot / CHUNK_SLOTS;
       uint16_t endChunk = toSlot / CHUNK_SLOTS;
       CacheEntry entries[CHUNK_SLOTS] = {};
       
       for (uint16_t chunk = startChunk; chunk <= endChunk; chunk++) {
           if (!loadChunk(chunk, entries)) return false;
           
           uint16_t slotBase = chunk * CHUNK_SLOTS;
           for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
               uint16_t slot = static_cast<uint16_t>(slotBase + i);
               
               // Guard clauses with clear names
               if (!isSlotInRange(slot, fromSlot, toSlot)) continue;
               
               const CacheEntry& entry = entries[i];
               if (!isSlotEligible(slot, entry, moduleId)) continue;
               
               cb(slot, entry, ctx);
           }
       }
       return true;
   }
   ```

**Estimated effort**: 45-55 minutes

## References
- Refactoring: "Decompose Conditional" (Fowler, 2018)
- Clean Code: "Functions should do one thing" (Martin, 2008)
- Guard clauses pattern: https://refactoring.com/catalog/replaceNestedConditionalWithGuardClauses.html
