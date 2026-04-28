---
title: "[MEDIUM] TropicStorage cache integration lacks tests for consistency and rebuild"
severity: MEDIUM
domain: core
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_core"
  - "area:storage"
---

## Summary
The `TropicStorage` component maintains an NVS-based cache of TROPIC01 R-Memory metadata, but **no integration tests** verify that the cache stays consistent with hardware state or that rebuild operations work correctly.

## Evidence

**TropicStorage API** (`components/cdc_core/include/cdc_core/TropicStorage.h:20-48`):
```cpp
class TropicStorage {
    bool isCacheValid() const;
    bool forEachSlot(uint8_t moduleId, SlotCallback cb, void* ctx);
    bool writeSlot(uint8_t moduleId, uint16_t slot, const char* name, uint8_t flags);
    bool rebuild();          // Rebuild cache from hardware
    bool rebuildVerbose(RebuildLogFn logFn, void* ctx);
    bool cleanup();
};
```

**Cache structure** (`components/cdc_core/include/cdc_core/TropicStorage.h:10-20`):
```cpp
struct CacheEntry {
    uint8_t moduleId;
    uint8_t flags;
    char name[16];
} __attribute__((packed));

struct CacheHeader {
    uint8_t version;
    uint8_t chunkSlots;
    uint16_t entrySize;
    uint32_t mapSignature;
} __attribute__((packed));
```

**Implementation details** (`components/cdc_core/src/TropicStorage.cpp:28-150`):
- Uses NVS namespace "tr01_meta" for cache
- Cache organized in 64-slot chunks
- Signature computed from slot map to detect changes
- Rebuild reads all R-Memory slots from hardware

**Usage in modules** (`components/mod_gpg/src/GpgStorage.cpp`):
```cpp
// Modules use TropicStorage to find their slots
auto& tropicStorage = TropicStorage::instance();
tropicStorage.forEachSlot(moduleId, onSlotFound, ctx);
```

**Current test coverage**: None

## Impact
- **Cache inconsistency**: NVS cache may not match hardware after direct writes
- **Rebuild failures**: Rebuild operation could fail silently
- **Corrupted cache**: Bad cache header could cause data loss
- **Module confusion**: Modules may not find their stored data

## Recommended Fix

Create integration test `test_tropic_storage_cache/` that verifies:

1. **Cache initialization**: Cache loads correctly from NVS
2. **Cache write**: writeSlot() updates cache and NVS
3. **Cache read**: forEachSlot() finds all written slots
4. **Cache rebuild**: rebuild() recovers from cache mismatch
5. **Cache validation**: Invalid cache detected and rebuilt
6. **Module isolation**: Modules only see their own slots

**Test structure** (example):
```cpp
// test/test_tropic_storage_cache/test_cache_ops.cpp
#include "cdc_core/TropicStorage.h"
#include "cdc_hal/ISecureElement.h"

void test_tropic_storage_write_read() {
    auto& storage = TropicStorage::instance();
    storage.init();
    storage.start();
    
    // Write slot
    storage.writeSlot(1, 5, "test_key", 0x01);
    
    // Read back
    bool found = false;
    storage.forEachSlot(1, [](uint16_t slot, const auto& entry, void* ctx) {
        found = true;
    }, nullptr);
    
    ASSERT_TRUE(found);
}

void test_tropic_storage_rebuild() {
    auto& storage = TropicStorage::instance();
    
    // Invalidate cache
    storage.rebuild();
    
    // Should still find slots from hardware
    ASSERT_TRUE(storage.isCacheValid());
}
```

## References
- [TropicStorage header](components/cdc_core/include/cdc_core/TropicStorage.h)
- [TropicStorage implementation](components/cdc_core/src/TropicStorage.cpp)
