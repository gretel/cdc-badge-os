---
title: "[MEDIUM] TROPIC storage rebuild runs synchronously during startup"
severity: MEDIUM
domain: startup-performance
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
The `TropicStorage::rebuild()` method iterates **all R-Memory slots** (up to 512 slots) on every boot if cache is invalidated, performing sequential SPI reads from the TROPIC01 chip. This can take **hundreds of milliseconds to seconds** depending on slot population.

**Location:** `components/cdc_core/src/TropicStorage.cpp:217-275`

## Impact
- **Variable boot time**: Cache miss = full rebuild = 512 × ~5ms = 2.5 seconds worst case
- **Blocking operation**: Complete system freeze during rebuild
- **SPI bus contention**: TROPIC01 SPI reads block other I2C devices (display, power management)
- **No progress indication**: User sees frozen display during rebuild

## Evidence
From `components/cdc_core/src/TropicStorage.cpp:217-275`:
```cpp
bool TropicStorage::rebuildVerbose(RebuildLogFn logFn, void* ctx) {
    // ... session start ...
    
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
            // ... read logic ...
        }
        if (!saveChunk(chunkIndex, chunk)) {  // NVS write per chunk!
            return false;
        }
    }
    // ...
}
```

Called from `cleanup()`:
```cpp
bool TropicStorage::cleanup() {
    // ... loop through chunks ...
    return rebuild();  // Full rebuild!
}
```

With 64 chunks (512 slots / 8 slots per chunk), each requiring:
- SPI read (~1-2ms per slot)
- NVS write (~10-20ms per chunk)

Total: **64 × (8 × 2ms + 15ms)** ≈ **2 seconds** worst case.

## Recommended Fix
1. **Incremental rebuild**: Only rebuild chunks that changed, not all chunks
2. **Lazy initialization**: Defer rebuild until first access, not at boot
3. **Progressive rebuild**: Split rebuild across multiple `onTick()` calls to allow UI updates

**Implementation:**
```cpp
// Add incremental tracking
class TropicStorage {
    uint16_t lastRebuildCount_;  // Slots counted at last rebuild
    uint32_t mapSignature_;      // Current slot map signature
    
    bool needsRebuild() const {
        return cacheValid_ == false || 
               mapSignature_ != computeMapSignature() ||
               lastRebuildCount_ != getCurrentSlotCount();
    }
    
    void incrementalRebuild() {
        // Only scan slots that might have changed
        // Use slot timestamps or version counters
    }
};

// In main.cpp, defer rebuild to background
tropicStorage.init();
tropicStorage.start();
// Don't call rebuild() immediately - do it lazily on first access
```

**Alternative**: Cache validation without full rebuild
```cpp
// Store slot count + checksum in header
struct CacheHeader {
    uint8_t version;
    uint16_t slotCount;
    uint32_t checksum;  // CRC of all slot headers
    // ...
};

// On boot, only read slot headers (4 bytes each) to validate
// Full rebuild only if checksum mismatch
```

## References
- TROPIC01 Datasheet: [R-Memory access timing](https://www.tropicdevices.com/)
- ESP-IDF SPI: [SPI bus sharing](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/spi_master.html)
