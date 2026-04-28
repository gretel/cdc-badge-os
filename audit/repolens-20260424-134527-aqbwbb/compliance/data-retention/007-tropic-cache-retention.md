---
title: "[LOW] TROPIC01 Cache Metadata Retention Without Invalidation Strategy"
severity: LOW
domain: compliance
lens: data-retention
labels:
  - "audit:compliance/data-retention"
---

## Summary
The TROPIC01 metadata cache (in `components/cdc_core/src/TropicStorage.cpp`) stores slot metadata (module ID, flags, name) in NVS for fast lookup. The cache is persisted in NVS namespace `tr01_meta` and is only rebuilt when the cache header is invalid or missing. There is no proactive cache invalidation strategy, TTL-based expiration, or periodic refresh mechanism.

**Location**: `components/cdc_core/src/TropicStorage.cpp` (lines 25-45, 204-274), `components/cdc_core/include/cdc_core/TropicStorage.h` (lines 54-59)

## Impact
1. **Stale Metadata Risk**: If TROPIC01 R-Memory is modified externally (e.g., via direct secure element commands), the NVS cache may become stale
2. **NVS Wear**: Cache rebuilds require reading all R-Memory slots and rewriting NVS, contributing to NVS wear
3. **No Forced Refresh**: No mechanism to force cache invalidation for troubleshooting
4. **Schema Drift**: Cache header has version field but no migration strategy if schema changes

## Evidence
**Cache structure** (`TropicStorage.h:54-59`):
```cpp
struct CacheHeader {
    uint8_t version;
    uint8_t chunkSlots;
    uint16_t entrySize;
    uint32_t mapSignature;
} __attribute__((packed));
```

**Cache validation** (`TropicStorage.cpp:38-42`):
```cpp
cacheValid_ = loadHeader();

if (!cacheValid_) {
    LOG_W(TAG, "Cache header missing or stale - rebuild required");
}
```

**Cache persistence**: Cache is stored in NVS namespace `tr01_meta` (line 12):
```cpp
static constexpr const char* NVS_NAMESPACE = "tr01_meta";
```

**Rebuild logic** (`TropicStorage.cpp:204-274`):
- Rebuilds entire cache from TROPIC01 R-Memory
- No incremental updates
- No TTL or age-based invalidation

**No cache invalidation command**: Serial commands documentation shows no `TR01_CACHE_CLEAR` or similar command to force cache rebuild.

## Recommended Fix
Add cache invalidation and lifecycle management:

**Option 1: Add cache age tracking**
Add timestamp to cache header:
```cpp
struct CacheHeader {
    uint8_t version;
    uint8_t chunkSlots;
    uint16_t entrySize;
    uint32_t mapSignature;
    uint32_t builtAt;       // Unix timestamp of last rebuild
    uint32_t maxAgeSecs;    // Optional TTL (0 = no expiry)
}
```

**Option 2: Add serial command for cache clear**
```bash
TR01_CACHE_CLEAR  # Invalidate cache, force rebuild on next access
```

**Option 3: Add periodic soft-invalidation**
Every N days, mark cache as "potentially stale" and log warning:
```cpp
static constexpr uint32_t CACHE_SOFT_EXPIRY_DAYS = 30;

void TropicStorage::checkCacheAge() {
    uint32_t now = esp_timer_get_time() / 1000000;  // seconds
    uint32_t age = now - header_.builtAt;
    if (age > CACHE_SOFT_EXPIRY_DAYS * 86400) {
        LOG_W(TAG, "Cache is %lu days old, consider rebuild", age / 86400);
    }
}
```

**Option 4: Incremental cache updates**
Instead of full rebuild, update only changed slots (requires tracking slot changes).

## References
- ESP-IDF NVS API for persistent storage
- Cache invalidation patterns (time-based, version-based, explicit)
- TROPIC01 secure element documentation

(End of file - total 112 lines)
