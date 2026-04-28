---
title: "[MEDIUM] NVS cache schema versioning lacks backward compatibility handling"
severity: MEDIUM
domain: database/schema-design
lens: schema-evolution
labels:
  - "storage"
  - "nvs"
  - "schema-versioning"
---

## Summary
The `TropicStorage` NVS cache in `TropicStorage.cpp` uses a `CacheHeader` with version checking, but when the version doesn't match, the entire cache is invalidated and requires a full rebuild. There's no migration path for existing cache data, and the schema evolution strategy is "drop and rebuild" which is inefficient for large slot counts.

**Evidence:**
- `TropicStorage.h:51-59`:
  ```cpp
  struct CacheHeader {
      uint8_t version;
      uint8_t chunkSlots;
      uint16_t entrySize;
      uint32_t mapSignature;
  };
  ```
- `TropicStorage.cpp:32-40`:
  ```cpp
  header_.version = CACHE_VERSION;
  header_.chunkSlots = CHUNK_SLOTS;
  header_.entrySize = sizeof(CacheEntry);
  header_.mapSignature = computeMapSignature();

  cacheValid_ = loadHeader();
  if (!cacheValid_) {
      LOG_W(TAG, "Cache header missing or stale - rebuild required");
  }
  ```
- `TropicStorage.cpp:334-340`:
  ```cpp
  if (stored.version != CACHE_VERSION || stored.chunkSlots != CHUNK_SLOTS ||
      stored.entrySize != sizeof(CacheEntry)) {
      return false;  // Complete cache invalidation!
  }
  ```

## Impact
1. **Performance**: Full rebuild of up to 512 R-Memory slots required on any schema change
2. **User experience**: First boot after firmware update requires slow rebuild process
3. **Wear**: Unnecessary flash writes during rebuild (R-Memory slots must be re-read and re-written)
4. **Scalability**: As more modules are added, rebuild time increases linearly

## Evidence
From `TropicStorage.cpp:334-340`:
```cpp
if (stored.version != CACHE_VERSION || stored.chunkSlots != CHUNK_SLOTS ||
    stored.entrySize != sizeof(CacheEntry)) {
    return false;  // No migration, just invalidation
}
```

From `TropicStorage.cpp:217-275`: `rebuildVerbose()` must read ALL 512 R-Memory slots:
```cpp
uint16_t totalChunks = static_cast<uint16_t>((TropicSlotMap::instance().rmemMax() + 1u) / CHUNK_SLOTS);
for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
    // Reads each slot from secure element
}
```

## Recommended Fix
1. **Implement incremental migration**: When version changes, only migrate affected chunks
2. **Add schema migration table**: Track which chunks have been migrated
3. **Preserve unchanged data**: If `moduleId` and `entrySize` compatible, keep existing entries
4. **Background migration**: Allow rebuild to happen incrementally over multiple boots

Example approach:
```cpp
struct CacheHeader {
    uint8_t version;
    uint8_t chunkSlots;
    uint16_t entrySize;
    uint32_t mapSignature;
    uint16_t migratedChunks;  // Bitmask of migrated chunks
    uint8_t migrationState;   // 0=none, 1=in-progress, 2=complete
};
```

## References
- SQLite schema evolution patterns
- Embedded NVS migration best practices
