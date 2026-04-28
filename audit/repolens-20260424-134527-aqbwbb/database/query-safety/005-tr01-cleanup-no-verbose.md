---
title: "[MEDIUM] TROPIC01 cleanup() erases slots without detailed logging"
severity: MEDIUM
domain: database
lens: query-safety
labels:
  - "audit:database/query-safety"
---

## Summary
The `TropicStorage::cleanup()` function at `components/cdc_core/src/TropicStorage.cpp:281-316` erases R-Memory slots that have module mismatches, but only logs a simple message. After erasing, it calls `rebuild()` which could potentially re-populate slots with data from the secure element, but there's no verification that the cleanup was successful or that data wasn't lost.

## Impact
- **Silent Data Loss**: If a slot is erased due to "mismatched module" but the data was valid, there's no way to recover it
- **Limited Diagnostics**: Only logs "slot X has mismatched module Y" without showing what data was in the slot
- **No Rollback**: Once erased, data is gone forever; no backup or verification step

## Evidence
File: `components/cdc_core/src/TropicStorage.cpp`

Lines 281-316:
```cpp
bool TropicStorage::cleanup() {
    if (!secureElement_) {
        LOG_E(TAG, "No secure element set");
        return false;
    }

    CacheEntry entries[CHUNK_SLOTS] = {};
    uint16_t totalChunks = static_cast<uint16_t>((TropicSlotMap::instance().rmemMax() + 1u) / CHUNK_SLOTS);

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
            secureElement_->rmemErase(slot);  // Erases data immediately!
            memset(&entry, 0, sizeof(entry));
            changed = true;
        }
        if (changed) {
            if (!saveChunk(chunkIndex, entries)) {
                return false;
            }
        }
    }

    return rebuild();  // Rebuilds from chip - but data is already gone!
}
```

The command handler at `SerialCmd.cpp:1113-1120`:
```cpp
static void cmdTr01Cleanup(const char* args) {
    (void)args;
    auto& storage = core::TropicStorage::instance();
    Console::printf("Cleaning TR01 cache + slots...\r\n");
    if (storage.cleanup()) {
        Console::printf("OK: Cleanup complete\r\n");
    } else {
        Console::printf("ERROR: Cleanup failed\r\n");
    }
}
```

## Recommended Fix
1. **Add verbose logging** to show what will be erased before erasing:
```cpp
bool TropicStorage::cleanup(bool verbose = false) {
    // ...
    for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
        // ...
        if (!isEntryAllowed(slot, entry.moduleId)) {
            if (verbose) {
                // Read and log actual data before erasing
                uint8_t data[256];
                uint16_t dataLen = 0;
                secureElement_->rmemRead(slot, data, sizeof(data), &dataLen);
                LOG_W(TAG, "Cleaning slot %u: module %u, %u bytes", slot, entry.moduleId, dataLen);
            }
            LOG_W(TAG, "Cleanup: slot %u has mismatched module %u", slot, entry.moduleId);
            secureElement_->rmemErase(slot);
            // ...
        }
    }
    // ...
}
```

2. **Add confirmation** for serial command:
```cpp
static void cmdTr01Cleanup(const char* args) {
    if (strcmp(args, "CONFIRM") != 0) {
        Console::printf("WARNING: TR01_CLEANUP will erase mismatched slots!\r\n");
        Console::printf("To proceed, type: TR01_CLEANUP CONFIRM\r\n");
        return;
    }
    // ...
}
```

3. **Add dry-run mode** to preview what will be cleaned:
```cpp
static void cmdTr01Cleanup(const char* args) {
    if (strcmp(args, "DRYRUN") == 0) {
        // Preview only, no erasing
    } else if (strcmp(args, "CONFIRM") == 0) {
        // Proceed with cleanup
    } else {
        Console::printf("Usage: TR01_CLEANUP [DRYRUN|CONFIRM]\r\n");
    }
}
```

## References
- TROPIC01 storage: `components/cdc_core/src/TropicStorage.cpp`
- Serial commands: `components/serial_cmd/src/SerialCmd.cpp:1113`
- Slot map: `main/tropic_slot_map.h`
