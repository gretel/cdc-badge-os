---
title: "[MEDIUM] TROPIC01 cleanup performs bulk erases without per-slot confirmation"
severity: MEDIUM
domain: database
lens: query-safety
labels:
  - audit:database/query-safety
---

## Summary
The `TropicStorage::cleanup()` function (line 281-315 in `components/cdc_core/src/TropicStorage.cpp`) automatically erases all R-Memory slots that have mismatched module IDs without individual confirmation. This is called by the `TR01_CLEANUP` command.

**Evidence:**
- File: `components/cdc_core/src/TropicStorage.cpp`
- Lines: 298-308
```cpp
for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
    uint16_t slot = static_cast<uint16_t>(slotBase + i);
    CacheEntry& entry = entries[i];
    if (!isEntryUsed(entry)) continue;
    if (isEntryAllowed(slot, entry.moduleId)) continue;

    LOG_W(TAG, "Cleanup: slot %u has mismatched module %u", slot, entry.moduleId);
    secureElement_->rmemErase(slot);  // Bulk erase without per-slot confirmation
    memset(&entry, 0, sizeof(entry));
    changed = true;
}
```

## Impact
- **Silent Data Loss**: All mismatched slots are erased automatically
- **No Recovery**: Once erased, R-Memory data is lost
- **Module Migration Risk**: If module slot ranges change, all data for that module could be wiped
- **Debug Difficulty**: Hard to track what was erased and why

## Recommended Fix
1. Add a dry-run mode to preview what will be erased
2. Log all slots to be erased before performing the operation
3. Consider making cleanup more selective (e.g., only erase truly orphaned slots)
4. Add a summary report after cleanup showing what was affected

**Option A - Dry-run mode:**
```cpp
bool TropicStorage::cleanup(bool dryRun = false) {
    // ...
    for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
        // ...
        if (isEntryAllowed(slot, entry.moduleId)) continue;

        if (dryRun) {
            LOG_W(TAG, "Would cleanup: slot %u (module %u)", slot, entry.moduleId);
        } else {
            LOG_W(TAG, "Cleanup: slot %u has mismatched module %u", slot, entry.moduleId);
            secureElement_->rmemErase(slot);
        }
        // ...
    }
}
```

**Option B - Summary report:**
```cpp
struct CleanupResult {
    uint16_t slotsFound;
    uint16_t slotsErased;
    uint16_t slotsSkipped;
};
CleanupResult TropicStorage::cleanup(bool dryRun = false);
```

## References
- Called by: `cmdTr01Cleanup` (line 1118 in SerialCmd.cpp)
- Slot mapping: `TropicSlotMap::isRmemAllowedForModuleId()`
