---
title: "[MEDIUM] TROPIC01 storage iteration loads full chunk into memory"
severity: MEDIUM
domain: performance/pagination
lens: pagination-streaming
labels:
  - "audit:performance/pagination"
---

## Summary
The `TropicStorage::forEachSlot()` function loads entire **64-slot chunks** (288 bytes each) into memory during iteration. While this is reasonable for the chunk size, the pattern could be improved for better memory efficiency and to support true streaming iteration.

**File**: `components/cdc_core/src/TropicStorage.cpp:87-120`
```cpp
bool TropicStorage::forEachSlot(uint8_t moduleId, uint16_t fromSlot, uint16_t toSlot,
                                SlotCallback cb, void* ctx) {
    // ...
    uint16_t startChunk = fromSlot / CHUNK_SLOTS;
    uint16_t endChunk = toSlot / CHUNK_SLOTS;

    CacheEntry entries[CHUNK_SLOTS] = {};  // 64 entries allocated on stack
    for (uint16_t chunk = startChunk; chunk <= endChunk; chunk++) {
        if (!loadChunk(chunk, entries)) {
            return false;
        }
        uint16_t slotBase = chunk * CHUNK_SLOTS;
        for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
            // ...
        }
    }
}
```

## Impact
- **Stack usage**: `CacheEntry entries[64]` uses ~1.8KB of stack space (64 × 28 bytes per entry)
- **NVS reads**: Full chunk loaded from NVS even if only a few slots are needed
- **No early termination**: Cannot stop iteration early without processing entire chunk
- **Callback design**: While callback-based, the full chunk is pre-loaded before any callback is invoked

## Evidence
**File**: `components/cdc_core/src/TropicStorage.cpp:87-120`
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
    // ...
    CacheEntry entries[CHUNK_SLOTS] = {};  // Stack allocation of 64 entries
    for (uint16_t chunk = startChunk; chunk <= endChunk; chunk++) {
        if (!loadChunk(chunk, entries)) {
            return false;
        }
        uint16_t slotBase = chunk * CHUNK_SLOTS;
        for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
            uint16_t slot = static_cast<uint16_t>(slotBase + i);
            if (slot < fromSlot || slot > toSlot) continue;
            const CacheEntry& entry = entries[i];
            if (!isEntryUsed(entry)) continue;
            if (!isEntryAllowed(slot, entry.moduleId)) continue;
            if (entry.moduleId != moduleId) continue;
            cb(slot, entry, ctx);  // Callback invoked after full chunk loaded
        }
    }
    return true;
}
```

**File**: `components/cdc_core/include/cdc_core/TropicStorage.h:18`
```cpp
static constexpr uint16_t CHUNK_SLOTS = 64;
```

**File**: `components/cdc_core/src/TropicStorage.cpp:368-388`
```cpp
bool TropicStorage::loadChunk(uint16_t chunkIndex, CacheEntry* entries) {
    if (!entries) return false;
    memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);  // Zero entire chunk

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return true;
    }
    char key[12];
    snprintf(key, sizeof(key), "c%u", chunkIndex);
    size_t len = sizeof(CacheEntry) * CHUNK_SLOTS;
    esp_err_t err = nvs_get_blob(nvs, key, entries, &len);  // Read full chunk from NVS
    nvs_close(nvs);
    // ...
}
```

## Recommended Fix
Implement **streaming iteration** at the NVS chunk level:

1. **Add streaming variant** of `forEachSlot()` that reads one entry at a time:
   ```cpp
   bool forEachSlotStreaming(uint8_t moduleId, uint16_t fromSlot, uint16_t toSlot,
                             SlotCallback cb, void* ctx);
   ```

2. **Read individual entries** from NVS instead of full chunks when iterating across chunk boundaries

3. **Support early termination** by allowing callback to return `false` to stop iteration:
   ```cpp
   using SlotCallback = bool(*)(uint16_t slot, const CacheEntry& entry, void* ctx);
   ```

4. **Optimize chunk caching**: Keep last-read chunk in cache to avoid re-reading when iterating sequentially

5. **Consider PSRAM** for chunk buffer if stack depth is a concern:
   ```cpp
   CacheEntry* entries = (CacheEntry*)heap_caps_malloc(
       sizeof(CacheEntry) * CHUNK_SLOTS, MALLOC_CAP_SPIRAM);
   ```

## References
- ESP32-S3 stack size: Typically 8KB per task
- NVS blob storage: Each chunk stored as single blob (~1.8KB)
- TROPIC01 has 512 R-Memory slots total, organized in 8 chunks of 64
