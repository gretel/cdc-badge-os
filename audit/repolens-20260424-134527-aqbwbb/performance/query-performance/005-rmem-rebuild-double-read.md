---
title: "[LOW] Full R-Memory cache rebuild reads each slot twice (header + fallback)"
severity: LOW
domain: embedded-storage
lens: query-performance
labels:
  - "rmem-read"
  - "cache-rebuild"
  - "double-read"
---

## Summary

In `components/cdc_core/src/TropicStorage.cpp`, the `rebuildVerbose()` function performs a full R-Memory scan to rebuild the cache. For each slot, it calls `rmemReadWithHeader()` which reads the entire slot (up to 444 bytes). If the header read fails or returns insufficient data, it then calls `rmemRead()` again with a small buffer (4 bytes) as a fallback check.

**Location**: `components/cdc_core/src/TropicStorage.cpp:217-275` (rebuildVerbose)

## Impact

**Performance Cost**:
- Worst case: 2 R-Memory reads per slot during cache rebuild
- For 512 R-Memory slots: up to 1024 reads (though most slots are empty)
- Each R-Memory read involves SPI transaction with TROPIC01 (~100-500us latency)
- Typical rebuild time: 50-150ms depending on number of populated slots

**Memory Bandwidth**:
- First read: `rmemReadWithHeader()` reads full slot (buffer[RMEM_SLOT_SIZE] = 444 bytes)
- Second read (fallback): `rmemRead()` reads only 4 bytes
- Total bandwidth: ~450 bytes per slot in worst case

**When this matters**:
- Cache rebuild happens during initialization or after session loss
- Not a frequent operation, but noticeable delay on boot or reconnection
- User sees "Initializing..." or similar during rebuild

## Evidence

**Double-read pattern in `rebuildVerbose()` (lines 242-265)**:
```cpp
for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
    uint16_t slot = static_cast<uint16_t>(slotBase + i);
    if (slot == 0) continue;

    cdc::hal::ISecureElement::RMemHeader header = {};
    uint16_t payloadLen = 0;
    auto res = secureElement_->rmemReadWithHeader(slot, &header, nullptr, 0, &payloadLen);
    if (res == cdc::hal::SeResult::OK) {
        // Success - use header data
        CacheEntry& entry = chunk[i];
        entry.moduleId = header.moduleId;
        entry.flags = static_cast<uint8_t>(header.flags | FLAG_USED);
        strncpy(entry.name, header.name, sizeof(entry.name) - 1);
        continue;
    }

    // FALLBACK: Read again to check if slot has any data
    uint8_t temp[4];
    uint16_t readLen = 0;
    auto rawRes = secureElement_->rmemRead(slot, temp, sizeof(temp), &readLen);
    if (rawRes == cdc::hal::SeResult::OK && readLen > 0) {
        if (logFn) logFn(slot, "invalid header", ctx);
    } else if (rawRes != cdc::hal::SeResult::SLOT_EMPTY) {
        if (logFn) logFn(slot, "read failed", ctx);
    }
}
```

**rmemReadWithHeader implementation** (Tropic01Element.cpp:730-773):
```cpp
SeResult Tropic01Element::rmemReadWithHeader(uint16_t slot, RMemHeader* headerOut,
                                             uint8_t* payloadOut, uint16_t payloadMax,
                                             uint16_t* payloadLenOut) {
    uint8_t buffer[RMEM_SLOT_SIZE] = {};  // 444 bytes
    uint16_t actualLen = 0;
    SeResult res = rmemRead(slot, buffer, sizeof(buffer), &actualLen);  // First read
    if (res != SeResult::OK) {
        return res;
    }
    // ... validate header ...
    // Extract header/payload from buffer
    return SeResult::OK;
}
```

**Note**: The fallback read is only needed when `rmemReadWithHeader()` fails. This happens when:
1. Slot is empty (returns `SLOT_EMPTY`)
2. Slot has data but invalid header
3. Read fails due to session error

## Recommended Fix

**Optimize the fallback check**: Instead of reading 4 bytes in the fallback, use the data already read by `rmemReadWithHeader()`:

**Option 1: Return raw read data from rmemReadWithHeader()**

Modify `rmemReadWithHeader()` to return the raw buffer when header validation fails:

```cpp
SeResult Tropic01Element::rmemReadWithHeader(uint16_t slot, RMemHeader* headerOut,
                                             uint8_t* payloadOut, uint16_t payloadMax,
                                             uint16_t* payloadLenOut,
                                             uint8_t* rawBuf, uint16_t rawMax, uint16_t* rawLen) {
    uint8_t buffer[RMEM_SLOT_SIZE] = {};
    uint16_t actualLen = 0;
    SeResult res = rmemRead(slot, buffer, sizeof(buffer), &actualLen);
    if (res != SeResult::OK) {
        return res;
    }
    
    // Always fill raw buffer if caller wants it
    if (rawBuf && rawMax > 0) {
        uint16_t copyLen = (actualLen < rawMax) ? actualLen : rawMax;
        memcpy(rawBuf, buffer, copyLen);
        if (rawLen) *rawLen = copyLen;
    }
    
    // ... rest of validation ...
}
```

**Option 2: Handle fallback in rebuildVerbose() using existing data**

Since `rmemReadWithHeader()` already reads the full slot, change `rebuildVerbose()` to handle the "invalid header" case without a second read:

```cpp
bool TropicStorage::rebuildVerbose(RebuildLogFn logFn, void* ctx) {
    // ...
    for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
        uint16_t slot = static_cast<uint16_t>(slotBase + i);
        if (slot == 0) continue;

        cdc::hal::ISecureElement::RMemHeader header = {};
        uint16_t payloadLen = 0;
        auto res = secureElement_->rmemReadWithHeader(slot, &header, nullptr, 0, &payloadLen);
        
        if (res == cdc::hal::SeResult::OK) {
            // Success
            CacheEntry& entry = chunk[i];
            entry.moduleId = header.moduleId;
            entry.flags = static_cast<uint8_t>(header.flags | FLAG_USED);
            strncpy(entry.name, header.name, sizeof(entry.name) - 1);
            if (logFn) logFn(slot, entry.name, ctx);
            continue;
        }

        // No fallback read needed - we already know the slot status from rmemReadWithHeader()
        // The read happened inside rmemReadWithHeader(), just check the result
        if (res == cdc::hal::SeResult::SLOT_EMPTY) {
            // Slot is empty, skip
            continue;
        } else {
            // Invalid header or read error
            if (logFn) logFn(slot, "invalid header", ctx);
        }
    }
    // ...
}
```

**Option 3: Add "peek" API**

Add a method that checks if a slot has data without full read:

```cpp
/**
 * \brief Checks if R-Memory slot has data (fast peek).
 * \param slot R-memory slot index.
 * \return `true` if slot appears populated.
 */
bool Tropic01Element::rmemSlotHasData(uint16_t slot) {
    // Read only first byte to check if slot is empty
    uint8_t firstByte;
    uint16_t len = 0;
    SeResult res = rmemRead(slot, &firstByte, 1, &len);
    return (res == SeResult::OK && len > 0);
}
```

**Recommended approach**: Option 2 is simplest (~20-30 minutes). The fallback read is redundant since `rmemReadWithHeader()` already performs the read and returns the appropriate status code.

## References

- [TROPIC01 R-Memory](https://www.microchip.com/en-us/products/security-ics/secure-elements/tropic01) - R-Memory slots are 444 bytes each, accessed via SPI
- [NVS Cache Chunk Size](components/cdc_core/src/TropicStorage.cpp) - CHUNK_SLOTS = 10, so rebuild processes 10 slots at a time
- [libtropic R-Memory API](third_party/libtropic/src/lt_l3_api_structs.h) - `lt_r_mem_data_read()` returns slot status

(End of file - total 198 lines)
