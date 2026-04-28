---
title: "[HIGH] TROPIC01 cache version migration lacks forward compatibility"
severity: HIGH
domain: database/migration-quality
lens: embedded-storage
labels:
  - "nvs-migration"
---

## Summary
In `components/cdc_core/src/TropicStorage.cpp:338`, the cache header validation performs an exact version match:

```cpp
if (stored.version != CACHE_VERSION || stored.chunkSlots != CHUNK_SLOTS ||
    stored.entrySize != sizeof(CacheEntry)) {
    return false;
}
```

When the version changes (e.g., from 1 to 2), the entire cache is invalidated and requires a full rebuild from the secure element. There is no migration path to transform old cache entries to the new format.

## Impact
- **Data loss risk**: If a new field is added to `CacheEntry`, all existing cache data is discarded and must be rebuilt from the slow TROPIC01 secure element (potentially hundreds of R-Memory slot reads).
- **User experience**: Every firmware update that modifies the cache structure forces a slow rebuild on the next boot.
- **Maintenance burden**: Future schema changes require careful planning since there's no incremental migration mechanism.

## Evidence
File: `components/cdc_core/src/TropicStorage.cpp:338`
```cpp
bool TropicStorage::loadHeader() {
    // ...
    if (stored.version != CACHE_VERSION || stored.chunkSlots != CHUNK_SLOTS ||
        stored.entrySize != sizeof(CacheEntry)) {
        return false;  // Complete cache invalidation
    }
    // ...
}
```

The `CacheEntry` struct (line 11-15) is packed with fixed sizes:
```cpp
struct CacheEntry {
    uint8_t moduleId;
    uint8_t flags;
    char name[cdc::hal::ISecureElement::RMEM_NAME_LEN];
} __attribute__((packed));
```

## Recommended Fix
Implement forward-compatible version handling:

1. **Add migration function**: Create `migrateCache(uint8_t oldVersion)` that can transform old cache formats.
2. **Version range support**: Accept versions >= 1 and dispatch to appropriate migration logic:
   ```cpp
   if (stored.version < CACHE_VERSION) {
       return migrateCache(stored.version);
   }
   ```
3. **Incremental migration**: For version 1→2, copy existing fields and zero-initialize new fields rather than full rebuild.
4. **Document migration path**: Add comments in `tropic_slot_map.h` describing what changes require version bumps.

## References
- ESP-IDF NVS documentation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api/storage/nvs.html
- Schema evolution best practices: https://martinfowler.com/articles/schema-evolution.html
