---
title: "[MEDIUM] TropicStorage::rebuild() lacks transaction safety for cache reconstruction"
severity: MEDIUM
domain: database/transaction-safety
lens: transaction-safety
labels:
  - "audit:database/transaction-safety"
---

## Summary

In `components/cdc_core/src/TropicStorage.cpp`, the `rebuildVerbose()` method iterates through all R-Memory slots and writes cache chunks to NVS without atomic transaction grouping. A power failure mid-rebuild can leave the cache in an inconsistent state where some chunks reflect R-Memory contents while others are stale or zeroed.

**Affected location:** `TropicStorage::rebuildVerbose()` (lines 212-275)

## Impact

**Partial cache rebuild scenario:**
1. Rebuild starts, processes chunk 0-5 (writes to NVS)
2. Power loss at chunk 6
3. On reboot: `loadHeader()` may succeed (if header was written) but chunks 6+ are stale
4. `forEachSlot()` returns inconsistent results: some slots from R-Memory, some from old cache

**Specific issues:**
- Header is written at the END of rebuild (line 273): `cacheValid_ = saveHeader();`
- If power fails before line 273, `cacheValid_` stays false, forcing next rebuild
- If header was cached from previous run but chunks are partial, validation passes but data is wrong
- `cleanup()` calls `rebuild()` at the end (line 316), compounding the risk

## Evidence

**TropicStorage::rebuildVerbose()** (lines 217-275):
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

            // ... read from R-Memory, populate chunk ...
        }
        if (!saveChunk(chunkIndex, chunk)) {  // Write each chunk immediately
            if (logFn) logFn(slotBase, "nvs write failed", ctx);
            return false;
        }
    }

    cacheValid_ = saveHeader();  // Header written LAST
    return cacheValid_;
}
```

**TropicStorage::loadHeader()** (lines 321-343):
```cpp
bool TropicStorage::loadHeader() {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return false;
    }
    size_t len = sizeof(CacheHeader);
    CacheHeader stored = {};
    esp_err_t err = nvs_get_blob(nvs, NVS_KEY_HEADER, &stored, &len);
    nvs_close(nvs);
    if (err != ESP_OK || len != sizeof(CacheHeader)) {
        return false;
    }

    if (stored.version != CACHE_VERSION || stored.chunkSlots != CHUNK_SLOTS ||
        stored.entrySize != sizeof(CacheEntry)) {
        return false;
    }
    if (stored.mapSignature != computeMapSignature()) {  // Signature check
        return false;
    }
    header_ = stored;
    return true;
}
```

The map signature is computed from `TropicSlotMap`, not from actual chunk contents, so it won't detect partial rebuilds.

## Recommended Fix

**Option 1: Write chunks to temporary keys, commit header last**
```cpp
bool TropicStorage::rebuildVerbose(RebuildLogFn logFn, void* ctx) {
    // ... session start ...

    // Write chunks with temporary keys (e.g., "c0_tmp", "c1_tmp")
    for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        // ... populate chunk ...
        char tmpKey[12];
        snprintf(tmpKey, sizeof(tmpKey), "c%u_tmp", chunkIndex);
        if (!saveChunkToKey(tmpKey, chunk)) {
            return false;
        }
    }

    // Write header with "rebuilding" flag
    header_.rebuilding = true;
    saveHeader();

    // Rename tmp chunks to final keys atomically (or just overwrite)
    for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        char tmpKey[12], finalKey[12];
        snprintf(tmpKey, sizeof(tmpKey), "c%u_tmp", chunkIndex);
        snprintf(finalKey, sizeof(finalKey), "c%u", chunkIndex);
        // NVS doesn't support rename, so write final key
        loadChunkFromKey(tmpKey, chunk);
        saveChunk(chunkIndex, chunk);
    }

    // Clear rebuilding flag
    header_.rebuilding = false;
    cacheValid_ = saveHeader();
    return cacheValid_;
}
```

**Option 2: Add chunk checksums**
Extend `CacheHeader` with a per-chunk checksum array:
```cpp
struct CacheHeader {
    uint8_t version;
    uint8_t chunkSlots;
    uint16_t entrySize;
    uint32_t mapSignature;
    uint32_t chunkChecksums[MAX_CHUNKS];  // Per-chunk validation
};
```

Validate each chunk on load:
```cpp
bool TropicStorage::loadChunk(uint16_t chunkIndex, CacheEntry* entries) {
    // ... load from NVS ...
    
    // Compute checksum of loaded data
    uint32_t computed = computeChecksum(entries, sizeof(CacheEntry) * CHUNK_SLOTS);
    if (computed != header_.chunkChecksums[chunkIndex]) {
        // Chunk is stale, zero it out
        memset(entries, 0, sizeof(CacheEntry) * CHUNK_SLOTS);
    }
    return true;
}
```

## References

- ESP32 NVS transaction safety: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html
- Write-ahead logging (WAL) pattern
- Checksum validation for stored data structures