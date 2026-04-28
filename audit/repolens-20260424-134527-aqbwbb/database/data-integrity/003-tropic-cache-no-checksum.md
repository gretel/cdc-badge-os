---
title: "[HIGH] TROPIC01 cache and hardware state can diverge without checksum validation"
severity: HIGH
domain: data-integrity
lens: database
labels:
  - tropic-storage
  - cache-coherence
  - data-corruption
---

## Summary
The TROPIC01 storage cache (NVS-backed) stores metadata about R-Memory slots but lacks a per-slot checksum to detect when the cached metadata doesn't match the actual hardware state. The cache uses a global signature based on the slot map, but this only detects slot-map changes, not individual slot corruption.

**Files:**
- `components/cdc_core/src/TropicStorage.cpp:21-77` (CacheEntry structure)
- `components/cdc_core/include/cdc_core/TropicStorage.h:11-16` (CacheEntry definition)
- `components/cdc_core/src/TropicStorage.cpp:217-270` (rebuildVerbose function)

## Impact
If NVS data becomes corrupted (bit flips, partial writes), the cache can show "used" slots that are actually empty in hardware, or vice versa. This leads to:
- False positives: System thinks a slot has data, but R-Memory read returns empty
- False negatives: System thinks a slot is free, but it contains valid data
- Silent data loss: Credentials/accounts appear to disappear

The rebuild function (`rebuildVerbose`) is the only recovery mechanism, but it's a fallback, not prevention.

## Evidence
The `CacheEntry` structure in `TropicStorage.h:11-16`:
```cpp
struct CacheEntry {
    uint8_t moduleId;
    uint8_t flags;
    char name[cdc::hal::ISecureElement::RMEM_NAME_LEN];
} __attribute__((packed));
```

No checksum/hash of the actual R-Memory content is stored. The cache only stores:
- Module ID
- Flags (used/not used)
- Name (from header)

The rebuild function at `TropicStorage.cpp:217-270` reads from hardware to rebuild cache:
```cpp
auto res = secureElement_->rmemReadWithHeader(slot, &header, nullptr, 0, &payloadLen);
if (res == cdc::hal::SeResult::OK) {
    // Update cache entry
    CacheEntry& entry = chunk[i];
    entry.moduleId = header.moduleId;
    entry.flags = static_cast<uint8_t>(header.flags | FLAG_USED);
    strncpy(entry.name, header.name, sizeof(entry.name) - 1);
}
```

But there's no validation that cached entries match hardware on normal reads.

## Recommended Fix
Add a content checksum to CacheEntry:

1. Extend `CacheEntry` to include a 16-bit checksum of the R-Memory header + payload
2. On each read, verify the checksum matches
3. If mismatch, mark entry invalid and trigger rebuild

```cpp
struct CacheEntry {
    uint8_t moduleId;
    uint8_t flags;
    char name[cdc::hal::ISecureElement::RMEM_NAME_LEN];
    uint16_t checksum;  // FNV-1a or CRC16 of slot contents
} __attribute__((packed));

// On write:
uint16_t calc_checksum(uint16_t slot) {
    // Read R-Memory, compute FNV-1a hash of header + payload
}

// On read:
if (entry.checksum != calc_checksum(slot)) {
    LOG_W(TAG, "Cache mismatch at slot %u", slot);
    return false;  // Trigger rebuild
}
```

Alternatively, store the R-Memory header's checksum field (already present in `RMemHeader.checksum`) in the cache and validate against it.

## References
- RMemHeader already has a checksum field: `ISecureElement.h:168`
- FIDO2 uses magic bytes for validation: `fido2_storage.cpp:33` (magic "FID2")
- See `TropicStorage::cleanup()` at line 276-318 for partial example of validation logic
