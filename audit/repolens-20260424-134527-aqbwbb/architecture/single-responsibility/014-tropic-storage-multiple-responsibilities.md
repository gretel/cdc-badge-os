---
title: "[MEDIUM] TropicStorage combines caching, iteration, NVS persistence, and secure-element access"
severity: MEDIUM
domain: cdc_core
lens: single-responsibility
labels:
  - "audit:architecture/single-responsibility"
  - "component:cdc_core"
---

## Summary
`components/cdc_core/src/TropicStorage.cpp` (479 lines) combines four distinct responsibilities:
1. **Cache management** - NVS header/chunk loading, cache validity tracking
2. **Slot iteration** - `forEachSlot()` with complex range filtering logic
3. **NVS persistence** - `writeSlot()`, `eraseSlot()` for metadata updates
4. **Secure-element access** - Direct TROPIC01 R-Memory reads in `loadChunk()`, `saveChunk()`

Key code evidence:
- Lines 19-60: Singleton init with cache header validation
- Lines 73-125: Complex slot iteration with module filtering and range bounds
- Lines 150-200: NVS chunk read/write operations
- Lines 200-280: Slot metadata read/write mixing NVS and TROPIC01 access
- Lines 280-350: Cache rebuild logic iterating TROPIC01 slots

## Impact
**Performance**: Cache rebuild requires full TROPIC01 scan, blocking UI thread because rebuild logic is embedded.

**Testability**: Testing cache logic requires TROPIC01 hardware or complex mocking of secure-element session.

**Coupling**: NVS cache format changes require modifying TROPIC01 slot iteration logic.

**Memory**: Cache entries stored in fixed-size array (64 entries per chunk) cannot be tuned independently of slot map structure.

## Evidence
File: `components/cdc_core/src/TropicStorage.cpp`

Lines 28-45 (Cache initialization):
```cpp
bool TropicStorage::init() {
    header_.version = CACHE_VERSION;
    header_.chunkSlots = CHUNK_SLOTS;
    header_.entrySize = sizeof(CacheEntry);
    header_.mapSignature = computeMapSignature();
    cacheValid_ = loadHeader();
    // ...
}
```

Lines 73-125 (Slot iteration with filtering):
```cpp
bool TropicStorage::forEachSlot(uint8_t moduleId, uint16_t fromSlot, uint16_t toSlot,
                                SlotCallback cb, void* ctx) {
    // Complex range calculation
    uint16_t startChunk = fromSlot / CHUNK_SLOTS;
    uint16_t endChunk = toSlot / CHUNK_SLOTS;
    for (uint16_t chunk = startChunk; chunk <= endChunk; chunk++) {
        if (!loadChunk(chunk, entries)) return false;
        // Multiple filtering steps
        for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
            if (!isEntryUsed(entry)) continue;
            if (!isEntryAllowed(slot, entry.moduleId)) continue;
            cb(slot, entry, ctx);
        }
    }
}
```

Lines 150-200 (NVS chunk operations):
```cpp
bool TropicStorage::loadChunk(uint16_t chunkIndex, CacheEntry* entries) {
    nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    char key[16];
    snprintf(key, sizeof(key), "chunk_%d", chunkIndex);
    nvs_get_str(nvs, key, buffer, &len);
    // Parse into entries
}
```

Lines 200-280 (Slot write with TROPIC01 access):
```cpp
bool TropicStorage::writeSlot(uint8_t moduleId, uint16_t slot, const char* name, uint8_t flags) {
    // Update NVS
    setEntry(slot, entry);
    saveChunk(...);
    // Also update TROPIC01 R-Memory
    secureElement_->writeRMemory(slot, ...);
}
```

## Recommended Fix
**Split into focused components** (each 1 hour task):

1. **Create `TropicCacheManager`**: Handles only NVS cache operations:
   - `loadHeader()`, `saveHeader()`
   - `loadChunk()`, `saveChunk()`
   - `computeMapSignature()`

2. **Create `TropicSlotIterator`**: Handles slot traversal:
   - `forEachSlot()` with range filtering
   - `getSlot()` resolution
   - Depends on `TropicSlotMap` for range info

3. **Create `TropicMetadataStore`**: Handles metadata persistence:
   - `writeSlot()`, `eraseSlot()`
   - NVS read/write for slot entries
   - Optional TROPIC01 sync

4. **Refactor `TropicStorage`**: Composes the three:
   - `init()` delegates to cache manager
   - `forEachSlot()` delegates to iterator
   - `writeSlot()` delegates to metadata store

**Files to create**:
- `components/cdc_core/include/cdc_core/TropicCacheManager.h`
- `components/cdc_core/include/cdc_core/TropicSlotIterator.h`
- `components/cdc_core/include/cdc_core/TropicMetadataStore.h`

**Migration steps**:
1. Create cache manager, move NVS operations
2. Create iterator, move slot iteration logic
3. Create metadata store, move persistence
4. Update `TropicStorage` to compose these

## References
- SRP: https://en.wikipedia.org/wiki/Single-responsibility_principle
- TROPIC01 R-Memory: https://www.tropic.works/products/tropic01/
- Caching patterns: https://www.oreilly.com/library/view/design-patterns-in/9781492034024/
