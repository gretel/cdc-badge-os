---
title: "[LOW] TropicStorage rebuild() blocks calling thread without progress callback"
severity: LOW
domain: concurrency/async-patterns
lens: async-patterns
labels:
  - "concurrency"
  - "blocking-io"
  - "storage"
  - "progress"
---

## Summary
In `components/cdc_core/src/TropicStorage.cpp`, the `rebuildVerbose()` method iterates through all R-Memory slots (up to 512) and performs synchronous reads from the secure element. This can take several seconds and blocks the calling thread. While it accepts a callback for progress logging, the operation is still synchronous and provides no way to cancel or check progress from outside.

**Location:** `components/cdc_core/src/TropicStorage.cpp:211-271`

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

    for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {  // 0 to ~32
        memset(chunk, 0, sizeof(chunk));
        uint16_t slotBase = chunkIndex * CHUNK_SLOTS;
        for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {  // 16 iterations per chunk
            uint16_t slot = static_cast<uint16_t>(slotBase + i);
            if (slot == 0) continue;

            // Synchronous read from secure element (~5-10ms per read)
            auto res = secureElement_->rmemReadWithHeader(slot, &header, nullptr, 0, &payloadLen);
            // ... processing ...
        }
        if (!saveChunk(chunkIndex, chunk)) {
            if (logFn) logFn(slotBase, "nvs write failed", ctx);
            return false;
        }
    }
    // Total: ~512 reads * 10ms = ~5 seconds blocking!

    cacheValid_ = saveHeader();
    return cacheValid_;
}
```

## Impact
- **Long blocking:** Rebuilding 512 slots can take 3-5 seconds, blocking the calling task
- **No cancellation:** No way to cancel the operation if it takes too long
- **No progress from outside:** Only internal logging, no way for UI to show progress
- **Called synchronously:** Typically called from main thread during boot, delaying system startup

## Evidence
File: `components/cdc_core/src/TropicStorage.cpp:211-271`
- Lines 220-265: Nested loops iterate all 512 R-Memory slots
- Line 235: `secureElement_->rmemReadWithHeader()` - synchronous SPI read (~5-10ms)
- Line 268: `saveChunk()` - synchronous NVS write (~5-10ms)
- Total operations: 512 reads + up to 32 NVS writes = ~5-7 seconds

Called from:
- `ModuleRegistry::runAllInitializers()` in `main.cpp` during boot
- `cleanup()` method which calls `rebuild()`

## Recommended Fix
**Option 1: Add non-blocking rebuild with progress tracking**
```cpp
// Add state enum
enum class RebuildState {
    IDLE,
    RUNNING,
    COMPLETED,
    CANCELLED
};

// Add state members
static RebuildState rebuildState_ = RebuildState::IDLE;
static uint16_t rebuildProgress_ = 0;
static RebuildLogFn rebuildLogFn_ = nullptr;
static void* rebuildCtx_ = nullptr;

// Non-blocking rebuild step
bool TropicStorage::rebuildStep(uint16_t* progressOut) {
    if (rebuildState_ == RebuildState::IDLE) {
        // Start rebuild
        rebuildState_ = RebuildState::RUNNING;
        rebuildProgress_ = 0;
        rebuildLogFn_ = nullptr;
        rebuildCtx_ = nullptr;
        
        // Initialize first chunk
        currentChunkIndex_ = 0;
        currentSlot_ = 1;
        // ... initialize state ...
    }

    if (rebuildState_ != RebuildState::RUNNING) {
        return false;
    }

    // Process one chunk per call (limit to ~50ms)
    for (int step = 0; step < 3; step++) {  // 3 chunks per call
        if (currentChunkIndex_ >= totalChunks_) {
            // Done
            rebuildState_ = RebuildState::COMPLETED;
            if (progressOut) *progressOut = 100;
            return true;
        }

        // Process current slot
        // ... existing logic ...
        
        rebuildProgress_ = (currentSlot_ * 100) / totalSlots_;
        if (progressOut) *progressOut = rebuildProgress_;
        
        // Yield if taking too long
        if (step >= 3) break;
    }

    return false;  // Not done yet
}

// Get progress
uint16_t getRebuildProgress() {
    return rebuildProgress_;
}

// Cancel rebuild
void cancelRebuild() {
    rebuildState_ = RebuildState::CANCELLED;
}
```

**Option 2: Run rebuild in background task**
```cpp
// Add task handle
static TaskHandle_t rebuildTask_ = nullptr;
static SemaphoreHandle_t rebuildMutex_ = nullptr;

// Background task
static void rebuildTaskFunc(void* arg) {
    auto* storage = static_cast<TropicStorage*>(arg);
    
    while (true) {
        xSemaphoreTake(rebuildMutex_, portMAX_DELAY);
        bool startRebuild = storage->rebuildPending_;
        storage->rebuildPending_ = false;
        xSemaphoreGive(rebuildMutex_);

        if (startRebuild) {
            storage->rebuildVerbose(nullptr, nullptr);
            // Notify completion
            xTaskNotify(rebuildTask_, 0, eNoAction);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Non-blocking trigger
void TropicStorage::rebuildAsync() {
    rebuildPending_ = true;
}

// Check completion
bool TropicStorage::isRebuildComplete() {
    // Check status from background task
}
```

**Option 3: Add yield points for main loop integration**
```cpp
// Simple yield-based approach
bool TropicStorage::rebuildStep() {
    if (!currentChunk_) {
        // Initialize
        currentChunk_ = 0;
        currentSlot_ = 1;
    }

    // Process one chunk
    if (currentChunk_ < totalChunks_) {
        // ... process chunk logic ...
        currentChunk_++;
        return false;  // Not done
    }

    // Finalize
    cacheValid_ = saveHeader();
    currentChunk_ = nullptr;
    return true;  // Done
}

// In main loop
if (!rebuildDone) {
    rebuildDone = TropicStorage::instance().rebuildStep();
    // Yield to other tasks
    vTaskDelay(pdMS_TO_TICKS(1));
}
```

## References
- [ESP32 NVS Write Performance](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)
- [TROPIC01 R-Memory Access Timing](https://www.tropic01.com/datasheet)
- [Coroutine patterns in embedded](https://www.embedded.com/design/programming-languages-and-compilers/4025678/Designing-a-main-loop-for-embedded-systems)
