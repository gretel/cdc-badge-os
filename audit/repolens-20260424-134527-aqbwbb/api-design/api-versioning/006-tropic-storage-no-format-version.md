---
title: "[HIGH] TROPIC01 Storage Cache Has Version but No Migration Strategy"
severity: HIGH
domain: API Design
lens: api-versioning
labels:
  - "audit:api-design/api-versioning"
---

## Summary
The `TropicStorage` cache has a version field (`components/cdc_core/src/TropicStorage.cpp:30`), but when the version changes, there's no migration strategy - it just invalidates the cache. This loses all cached metadata.

**Evidence:**
- `components/cdc_core/src/TropicStorage.cpp:30` - `static constexpr uint8_t CACHE_VERSION = 1;`
- `components/cdc_core/include/cdc_core/TropicStorage.h` - Header struct has version but no migration logic
- Cache invalidation on version mismatch without data preservation

## Impact
- When cache schema changes, all cached slot metadata is lost
- Requires full rescan of TROPIC01 (slow operation)
- Users may lose configuration if cache rebuild fails

## Evidence
Cache version check (from TropicStorage.cpp):
```cpp
static constexpr uint8_t CACHE_VERSION = 1;

// In loadCache():
if (stored.version != CACHE_VERSION || stored.chunkSlots != CHUNK_SLOTS ||
    stored.rmemSlots != RMEM_SLOTS) {
    // Cache invalidation - NO MIGRATION
    return false;
}
```

When version changes, the entire cache is discarded.

## Recommended Fix
Implement cache migration:

1. Add migration function:
```cpp
/**
 * Migrate cache from old version to current
 * @param oldVersion Source version
 * @param oldCache Old cache data
 * @return true if migration successful
 */
bool migrateCache(uint8_t oldVersion, const CacheHeader& oldCache);
```

2. Handle version differences:
```cpp
switch (stored.version) {
    case 1:
        cache = migrateV1ToCurrent(stored);
        break;
    case 2:
        cache = migrateV2ToCurrent(stored);
        break;
    default:
        // Unknown version, full rebuild needed
        return false;
}
```

3. Add migration logging:
```cpp
LOG_I(TAG, "Migrating cache from v%d to v%d", stored.version, CACHE_VERSION);
```

4. Document cache format changes in CHANGELOG

## References
- Database Migration Patterns: https://martinfowler.com/eaaDev/DatabaseScript.html
- Cache Versioning: https://aws.amazon.com/blogs/database/versioning-your-database-schema/
