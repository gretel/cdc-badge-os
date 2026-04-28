---
title: "[LOW] TROPIC01 R-Memory slot 0 used for system PIN - no module isolation"
severity: LOW
domain: database
lens: query-safety
labels:
  - "audit:database/query-safety"
---

## Summary
The TROPIC01 secure element's R-Memory slot 0 is used for system-wide PIN/lockout data (`tr01_meta` namespace in NVS), but this slot doesn't follow the module-isolation pattern used for other slots. This creates a potential for accidental overwrites if module ID validation is bypassed.

## Impact
- **Data Collision Risk**: If module ID allocation is misconfigured, a module could accidentally write to slot 0
- **System State Corruption**: PIN data and lockout state could be overwritten by module data
- **Inconsistent Pattern**: Other slots (1-511) are allocated to specific modules via `TropicSlotMap`

## Evidence
File: `components/cdc_core/src/TropicStorage.cpp`

Lines 277-316 (cleanup function shows slot validation):
```cpp
bool TropicStorage::cleanup() {
    // ...
    for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        if (!loadChunk(chunkIndex, entries)) {
            return false;
        }
        uint16_t slotBase = chunkIndex * CHUNK_SLOTS;
        bool changed = false;
        for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
            uint16_t slot = static_cast<uint16_t>(slotBase + i);
            CacheEntry& entry = entries[i];
            if (!isEntryUsed(entry)) continue;
            if (isEntryAllowed(slot, entry.moduleId)) continue;

            LOG_W(TAG, "Cleanup: slot %u has mismatched module %u", slot, entry.moduleId);
            secureElement_->rmemErase(slot);  // Erases mismatched slots
            memset(&entry, 0, sizeof(entry));
            changed = true;
        }
        // ...
    }
    return rebuild();
}
```

File: `main/tropic_slot_map.h` (slot allocation - need to check):

The system PIN is stored in NVS namespace `tr01_meta` (line 12 of TropicStorage.cpp):
```cpp
static constexpr const char* NVS_NAMESPACE = "tr01_meta";
static constexpr const char* NVS_KEY_HEADER = "hdr";
```

But slot 0 is referenced as system slot in comments:
- `SerialCmd.cpp:975`: "Slot 0:        System PIN/lockout"
- This suggests slot 0 has special meaning but may not be properly isolated

## Recommended Fix
1. **Reserve slot 0 explicitly** in `TropicSlotMap`:
```cpp
// In tropic_slot_map.h
bool isRmemAllowedForModuleId(uint16_t slot, uint8_t moduleId) const {
    // Slot 0 is reserved for system data
    if (slot == 0) {
        return moduleId == SYSTEM_MODULE_ID;  // Define SYSTEM_MODULE_ID = 0
    }
    // ... rest of validation
}
```

2. **Add validation** in `TropicStorage::writeSlot()`:
```cpp
bool TropicStorage::writeSlot(uint8_t moduleId, uint16_t slot, const char* name, uint8_t flags) {
    if (!isEntryAllowed(slot, moduleId)) {
        return false;
    }
    // Add special check for slot 0
    if (slot == 0 && moduleId != 0) {  // 0 = SYSTEM
        LOG_W(TAG, "Slot 0 reserved for system");
        return false;
    }
    // ...
}
```

3. **Document** slot 0 as reserved in `TropicSlotMap.h`

## References
- TROPIC01 slot allocation: `main/tropic_slot_map.h`
- Storage implementation: `components/cdc_core/src/TropicStorage.cpp`
- Module registry: `components/cdc_core/ModuleRegistry.h`
