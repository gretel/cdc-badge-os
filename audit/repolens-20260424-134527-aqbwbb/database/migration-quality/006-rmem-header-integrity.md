---
title: "[MEDIUM] TROPIC01 R-Memory header lacks integrity validation on read"
severity: MEDIUM
domain: database/migration-quality
lens: embedded-storage
labels:
  - "data-integrity"
  - "tropic01"
---

## Summary
In `components/cdc_core/src/TropicStorage.cpp:217-274`, the `rebuildVerbose()` function reads R-Memory headers from the secure element but only checks for read success:

```cpp
cdc::hal::ISecureElement::RMemHeader header = {};
uint16_t payloadLen = 0;
auto res = secureElement_->rmemReadWithHeader(slot, &header, nullptr, 0, &payloadLen);
if (res == cdc::hal::SeResult::OK) {
    if (!isEntryAllowed(slot, header.moduleId)) {
        if (logFn) logFn(slot, "mismatched module", ctx);
        continue;
    }
    // ... use header without checksum validation
}
```

The R-Memory header includes a checksum field (`hdr_checksum` per the design doc), but it's never validated when reading.

## Impact
- **Silent corruption**: If an R-Memory header is corrupted (bit flip, partial write), the system accepts it as valid.
- **Cache pollution**: Corrupted entries are written to the NVS cache, potentially causing iteration issues.
- **Debug difficulty**: When data appears wrong, there's no way to tell if it's corruption or a valid entry with unexpected values.

## Evidence
File: `components/cdc_core/src/TropicStorage.cpp:229-245`

The design document `docs/plans/2026-01-28-tropic-storage-design.md:44-49` specifies:
```
Header layout (22 bytes):
- `magic` (1 byte)
- `hdr_checksum` (1 byte) — 8-bit checksum over the remaining header fields
- `module_id` (1 byte)
- `flags` (1 byte)
- `name[16]` (16 bytes, null-terminated)
- `payload_len` (2 bytes, little-endian)
```

But in `rebuildVerbose()`, only the `moduleId` is validated:
```cpp
if (!isEntryAllowed(slot, header.moduleId)) {
    if (logFn) logFn(slot, "mismatched module", ctx);
    continue;
}

CacheEntry& entry = chunk[i];
entry.moduleId = header.moduleId;
entry.flags = static_cast<uint8_t>(header.flags | FLAG_USED);
strncpy(entry.name, header.name, sizeof(entry.name) - 1);
```

No checksum validation is performed.

## Recommended Fix
Add header checksum validation:

1. **Define checksum function**: Create a helper to compute the 8-bit checksum:
   ```cpp
   static uint8_t computeHeaderChecksum(const ISecureElement::RMemHeader& header) {
       uint8_t sum = 0;
       // Sum all bytes except checksum itself
       sum += header.magic;
       sum += header.moduleId;
       sum += header.flags;
       // ... add name bytes
       sum += header.payload_len & 0xFF;
       sum += (header.payload_len >> 8) & 0xFF;
       return sum;
   }
   ```

2. **Validate on read**: Check checksum after reading:
   ```cpp
   if (computeHeaderChecksum(header) != header.hdr_checksum) {
       LOG_W(TAG, "Slot %d: checksum mismatch", slot);
       continue;
   }
   ```

3. **Add magic validation**: Also verify the magic byte:
   ```cpp
   if (header.magic != EXPECTED_MAGIC) {
       LOG_W(TAG, "Slot %d: invalid magic", slot);
       continue;
   }
   ```

## References
- Data integrity patterns: https://www.cockroachlabs.com/docs/data-integrity
- ESP32 memory corruption: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/memory.html
