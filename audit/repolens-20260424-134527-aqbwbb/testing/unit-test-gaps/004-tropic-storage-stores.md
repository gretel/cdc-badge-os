---
title: "[HIGH] TropicStorage Cache Management Lacks Unit Test Coverage"
severity: HIGH
domain: Testing
lens: unit-test-gaps
labels:
  - "audit:testing/unit-test-gaps"
---

## Summary
The `TropicStorage` class (`components/cdc_core/src/TropicStorage.cpp`, 479 lines) manages the TROPIC01 secure element R-Memory metadata cache with no unit tests. Critical untested functions include:

- `init()` (line 28) - Cache header initialization/validation
- `loadHeader()` (line 322) - NVS header load with signature validation
- `saveHeader()` (line 346) - NVS header persistence
- `loadChunk()` (line 369) - Chunk load from NVS
- `saveChunk()` (line 396) - Chunk save to NVS
- `forEachSlot()` (line 87) - Slot iteration with bounds filtering
- `getSlot()` (line 138) - Index-to-slot resolution
- `writeSlot()` (line 166) - Cache entry write
- `eraseSlot()` (line 193) - Cache entry clear
- `rebuild()` (line 207) - Full cache rebuild from hardware
- `rebuildVerbose()` (line 217) - Verbose rebuild with callback
- `cleanup()` (line 281) - Remove invalid entries

## Impact
**Data Integrity Risk:** TropicStorage caches metadata for 500+ secure element slots:
1. Cache header validation (version, chunk size, signature) is untested
2. Chunk load/save with NVS blob API has untested error paths
3. `forEachSlot()` complex logic (module filtering, bounds, chunk iteration) is unproven
4. `rebuildVerbose()` callback handling is untested
5. `cleanup()` slot erasure logic could have side effects

## Evidence
File: `components/cdc_core/src/TropicStorage.cpp`

Line 28-46: `init()` - Cache validation
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

Line 87-125: `forEachSlot()` - Complex iteration logic
```cpp
// Gets module range from TropicSlotMap
if (!slotMap.getRangeByModuleId(moduleId, TropicSlotMap::SlotType::RMEM, &range)) {
    return false;
}
// Adjusts bounds
if (fromSlot == 0 || toSlot == 0xFFFF) {
    fromSlot = range.start;
    toSlot = range.end;
}
// Iterates chunks
for (uint16_t chunk = startChunk; chunk <= endChunk; chunk++) {
    if (!loadChunk(chunk, entries)) {
        return false;
    }
    // Filters entries
    for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
        if (!isEntryUsed(entry)) continue;
        if (!isEntryAllowed(slot, entry.moduleId)) continue;
        if (entry.moduleId != moduleId) continue;
        cb(slot, entry, ctx);
    }
}
```

Line 217-272: `rebuildVerbose()` - Hardware iteration with callback
```cpp
for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
    for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
        auto res = secureElement_->rmemReadWithHeader(slot, &header, ...);
        if (res == cdc::hal::SeResult::OK) {
            if (!isEntryAllowed(slot, header.moduleId)) {
                if (logFn) logFn(slot, "mismatched module", ctx);
                continue;
            }
            // Populates entry
            continue;
        }
        // Error handling
        if (logFn) logFn(slot, "invalid header", ctx);
    }
    if (!saveChunk(chunkIndex, chunk)) {
        if (logFn) logFn(slotBase, "nvs write failed", ctx);
        return false;
    }
}
```

Current test coverage:
```bash
$ find test/ -name "*.cpp" -exec grep -l "TropicStorage" {} \;
# Returns nothing - no TropicStorage tests exist
```

## Recommended Fix
Create `test/test_tropic_storage/test_tropic_storage.cpp` with test cases:

1. **Header tests:**
   - Test `loadHeader()` with valid/invalid version
   - Test `loadHeader()` with stale signature
   - Test `saveHeader()` persistence

2. **Chunk tests:**
   - Test `loadChunk()` with empty NVS returns zeros
   - Test `saveChunk()` and `loadChunk()` roundtrip
   - Test chunk boundary handling

3. **Iteration tests:**
   - Test `forEachSlot()` with module filter
   - Test `forEachSlot()` with slot bounds
   - Test `getSlot()` index resolution

4. **Entry tests:**
   - Test `writeSlot()` creates entry
   - Test `eraseSlot()` clears entry
   - Test `isEntryUsed()` flag checking

5. **Rebuild tests:**
   - Test `rebuild()` with mock secure element
   - Test `rebuildVerbose()` callback invocation

Example test:
```cpp
void test_writeSlot_creates_entry() {
    auto& store = TropicStorage::instance();
    store.setSecureElement(mockSE);
    
    bool result = store.writeSlot(MODULE_ID_FIDO2, 10, "key1", FLAG_USED);
    TEST_ASSERT_TRUE(result);
    
    CacheEntry entry;
    store.getEntry(10, &entry);
    TEST_ASSERT_EQUAL(MODULE_ID_FIDO2, entry.moduleId);
    TEST_ASSERT_EQUAL_STRING("key1", entry.name);
}

void test_foreachSlot_filters_by_module() {
    auto& store = TropicStorage::instance();
    int count = 0;
    store.writeSlot(MODULE_ID_FIDO2, 10, "key1", FLAG_USED);
    store.writeSlot(MODULE_ID_GPG, 11, "key2", FLAG_USED);
    
    store.forEachSlot(MODULE_ID_FIDO2, [&](uint16_t slot, const CacheEntry& e, void*) {
        count++;
    });
    
    TEST_ASSERT_EQUAL(1, count);  // Only FIDO2 entry
}
```

## References
- File: `components/cdc_core/include/cdc_core/TropicStorage.h` - Full API
- File: `components/cdc_core/include/cdc_core/TropicSlotMap.h` - Slot mapping
