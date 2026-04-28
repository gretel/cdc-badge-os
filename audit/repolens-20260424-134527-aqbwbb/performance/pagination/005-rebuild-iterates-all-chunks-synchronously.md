---
title: "[LOW] TROPIC rebuild iterates all 512 slots synchronously"
severity: LOW
domain: performance/pagination
lens: pagination-streaming
labels:
  - "audit:performance/pagination"
---

## Summary
The `TropicStorage::rebuildVerbose()` function iterates through all 512 R-Memory slots and their chunks synchronously, reading from the TROPIC01 secure element for each slot. This can be time-consuming and blocks other operations during the rebuild.

**File**: `components/cdc_core/src/TropicStorage.cpp:217-270`
```cpp
bool TropicStorage::rebuildVerbose(RebuildLogFn logFn, void* ctx) {
    // ...
    CacheEntry chunk[CHUNK_SLOTS] = {};
    uint16_t totalChunks =
        static_cast<uint16_t>((TropicSlotMap::instance().rmemMax() + 1u) / CHUNK_SLOTS);

    for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        memset(chunk, 0, sizeof(chunk));
        uint16_t slotBase = chunkIndex * CHUNK_SLOTS;
        for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
            uint16_t slot = static_cast<uint16_t>(slotBase + i);
            if (slot == 0) continue;

            cdc::hal::ISecureElement::RMemHeader header = {};
            uint16_t payloadLen = 0;
            auto res = secureElement_->rmemReadWithHeader(slot, &header, nullptr, 0, &payloadLen);
            // ...
        }
        if (!saveChunk(chunkIndex, chunk)) {
            // ...
        }
    }
    // ...
}
```

## Impact
- **Time cost**: 512 slots × ~1-5ms per read = 0.5-2.5 seconds total
- **Blocking I/O**: Secure element reads are synchronous
- **No incremental progress**: Cannot pause/resume rebuild
- **Full NVS write**: Each chunk written to NVS after processing

## Evidence
**File**: `components/cdc_core/src/TropicStorage.cpp:217-270`
```cpp
bool TropicStorage::rebuildVerbose(RebuildLogFn logFn, void* ctx) {
    if (!secureElement_) {
        LOG_E(TAG, "No secure element set");
        return false;
    }
    if (!secureElement_->isSessionActive()) {
        if (!secureElement_->sessionStart()) {
            if (logFn) logFn(0xFFFF, "session start failed", ctx);
            return false;
        }
    }

    CacheEntry chunk[CHUNK_SLOTS] = {};
    uint16_t totalChunks =
        static_cast<uint16_t>((TropicSlotMap::instance().rmemMax() + 1u) / CHUNK_SLOTS);

    for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {  // 8 chunks
        memset(chunk, 0, sizeof(chunk));
        uint16_t slotBase = chunkIndex * CHUNK_SLOTS;
        for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {  // 64 slots per chunk
            uint16_t slot = static_cast<uint16_t>(slotBase + i);
            if (slot == 0) continue;

            cdc::hal::ISecureElement::RMemHeader header = {};
            uint16_t payloadLen = 0;
            auto res = secureElement_->rmemReadWithHeader(slot, &header, nullptr, 0, &payloadLen);
            if (res == cdc::hal::SeResult::OK) {
                // Process entry
                continue;
            }
            // ...
        }
        if (!saveChunk(chunkIndex, chunk)) {  // NVS write per chunk
            if (logFn) logFn(slotBase, "nvs write failed", ctx);
            return false;
        }
    }

    cacheValid_ = saveHeader();
    return cacheValid_;
}
```

**File**: `components/cdc_core/include/cdc_core/TropicStorage.h:18`
```cpp
static constexpr uint16_t CHUNK_SLOTS = 64;
```

**File**: `components/cdc_core/include/cdc_core/TropicSlotMap.h` (implied):
- TROPIC01 has 512 R-Memory slots (0-511)
- 8 chunks of 64 slots each

## Recommended Fix
Implement **incremental rebuild** with pagination:

1. **Add incremental rebuild API**:
   ```cpp
   struct RebuildState {
       uint16_t currentChunk;
       uint16_t currentSlot;
       CacheEntry chunk[CHUNK_SLOTS];
   };
   
   bool rebuildStep(RebuildState* state, RebuildLogFn logFn, void* ctx);
   ```

2. **Resume from last position** instead of restarting:
   ```cpp
   bool rebuildResume(RebuildState* state, RebuildLogFn logFn, void* ctx);
   ```

3. **Save progress to NVS** during long rebuilds:
   ```cpp
   bool saveRebuildProgress(uint16_t chunkIndex, uint16_t slotIndex);
   ```

4. **Use task yielding** for long operations:
   ```cpp
   for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
       // ...
       if (i % 16 == 0) {
           vTaskDelay(1);  // Yield every 16 slots
       }
   }
   ```

5. **Cache optimization**: Only rebuild chunks that have changed (track modification time)

## References
- TROPIC01 I2C interface: ~100-400kHz bus speed
- NVS commit: Synchronous flash write (~1-5ms)
- ESP32-S3 FreeRTOS: Task priority and yield functions available
