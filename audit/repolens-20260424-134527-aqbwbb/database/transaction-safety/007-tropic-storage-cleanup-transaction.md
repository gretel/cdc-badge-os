---
title: "[LOW] TropicStorage::cleanup() performs R-Memory erase and cache update without atomicity"
severity: LOW
domain: database/transaction-safety
lens: transaction-safety
labels:
  - "audit:database/transaction-safety"
---

## Summary

In `components/cdc_core/src/TropicStorage.cpp`, the `cleanup()` method iterates through cache entries, erases R-Memory slots for mismatched entries, and updates the cache. If power fails between the R-Memory erase and cache update, the cache will show a slot as "used" while R-Memory is empty.

**Affected location:** `TropicStorage::cleanup()` (lines 277-316)

## Impact

**Inconsistent state scenario:**
1. Cleanup finds slot 10 with mismatched module ID
2. `se->rmemErase(10)` succeeds (slot is now empty)
3. Cache entry is cleared: `memset(&entry, 0, sizeof(entry))`
4. `saveChunk()` is called at line 309
5. Power fails during/after `saveChunk()` but before it completes
6. Result: R-Memory is empty, but cache chunk still has old entry

On next load:
- Cache shows slot 10 as "used" (old entry)
- R-Memory read returns empty slot
- `forEachSlot()` returns stale entry

## Evidence

**TropicStorage::cleanup()** (lines 277-316):
```cpp
bool TropicStorage::cleanup() {
    if (!secureElement_) {
        LOG_E(TAG, "No secure element set");
        return false;
    }

    CacheEntry entries[CHUNK_SLOTS] = {};
    uint16_t totalChunks =
        static_cast<uint16_t>((TropicSlotMap::instance().rmemMax() + 1u) / CHUNK_SLOTS);

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
            secureElement_->rmemErase(slot);  // First operation
            memset(&entry, 0, sizeof(entry));
            changed = true;
        }
        if (changed) {
            if (!saveChunk(chunkIndex, entries)) {  // Second operation
                return false;
            }
        }
    }

    return rebuild();  // Third operation!
}
```

The method chains three operations:
1. R-Memory erase (line 306)
2. Cache chunk save (line 309)
3. Full rebuild (line 316)

Each step can fail independently, leaving partial state.

## Recommended Fix

**Add error handling and logging for rollback scenarios:**
```cpp
bool TropicStorage::cleanup() {
    if (!secureElement_) {
        LOG_E(TAG, "No secure element set");
        return false;
    }

    CacheEntry entries[CHUNK_SLOTS] = {};
    uint16_t totalChunks =
        static_cast<uint16_t>((TropicSlotMap::instance().rmemMax() + 1u) / CHUNK_SLOTS);

    uint16_t cleanedCount = 0;
    for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        if (!loadChunk(chunkIndex, entries)) {
            LOG_E(TAG, "Failed to load chunk %u", chunkIndex);
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
            auto res = secureElement_->rmemErase(slot);
            if (res != cdc::hal::SeResult::OK) {
                LOG_W(TAG, "Failed to erase slot %u: %d", slot, res);
                // Continue with next slot, don't fail whole cleanup
                continue;
            }
            memset(&entry, 0, sizeof(entry));
            changed = true;
            cleanedCount++;
        }
        if (changed) {
            if (!saveChunk(chunkIndex, entries)) {
                LOG_E(TAG, "Failed to save chunk %u after cleanup", chunkIndex);
                // Note: R-Memory is already erased, cache is stale
                // Rebuild will fix this
            }
        }
    }

    LOG_I(TAG, "Cleanup removed %u mismatched entries", cleanedCount);
    return rebuild();
}
```

**Option 2: Add validation helper**
Create a function to validate cache vs R-Memory consistency:
```cpp
bool TropicStorage::validateCache() {
    // Compare cache entries with R-Memory headers
    // Return list of mismatches
}
```

Call this periodically or on boot to detect and fix inconsistencies.

## References

- TROPIC01 R-Memory erase semantics
- NVS chunk storage format
- Cache invalidation patterns