---
title: "[LOW] TROPICStorage rebuild skips slot 0 but doesn't explain why"
severity: LOW
domain: cdc_core/TropicStorage
lens: edge-case-testing
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `TropicStorage.cpp` (file: `components/cdc_core/src/TropicStorage.cpp:230-275`), the `rebuildVerbose` function skips slot 0 during the rebuild process, but the reason for this is not documented. This could be a legitimate optimization or a potential edge case bug.

Lines 233-239:
```cpp
for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
    memset(chunk, 0, sizeof(chunk));
    uint16_t slotBase = chunkIndex * CHUNK_SLOTS;
    for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
        uint16_t slot = static_cast<uint16_t>(slotBase + i);
        if (slot == 0) continue;  // Line 238 - Why skip slot 0?
```

The `if (slot == 0) continue;` at line 238 skips slot 0, but:
- There's no comment explaining why
- It's unclear if this is intentional (slot 0 reserved for system use) or an oversight
- The same pattern may exist elsewhere in the codebase

## Impact
- **Data loss**: If slot 0 is supposed to be rebuildable, data might be lost
- **Maintenance confusion**: Future developers may not understand why slot 0 is skipped
- **Edge case**: The boundary condition of slot 0 being special may need explicit handling

## Evidence
File: `components/cdc_core/src/TropicStorage.cpp`, lines 233-239

```cpp
for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
    memset(chunk, 0, sizeof(chunk));
    uint16_t slotBase = chunkIndex * CHUNK_SLOTS;
    for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
        uint16_t slot = static_cast<uint16_t>(slotBase + i);
        if (slot == 0) continue;  // Line 238 - No explanation

        cdc::hal::ISecureElement::RMemHeader header = {};
        uint16_t payloadLen = 0;
        auto res = secureElement_->rmemReadWithHeader(slot, &header, nullptr, 0, &payloadLen);
```

Checking the slot mapping documentation in `TropicSlotMap.h`:
```cpp
// System/R-Memory slot allocation:
// Slot 0: PINs (system)
// Slots 1-3: GPG
// Slot 4: CA
// Slots 5-31: FIDO2
// Slots 32-131: TOTP
// Slots 150-511: Passwords
```

Slot 0 is reserved for system PINs, which explains why it's skipped. However, this should be documented.

## Recommended Fix
Add explicit documentation for why slot 0 is skipped:

```cpp
for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
    memset(chunk, 0, sizeof(chunk));
    uint16_t slotBase = chunkIndex * CHUNK_SLOTS;
    for (uint16_t i = 0; i < CHUNK_SLOTS; i++) {
        uint16_t slot = static_cast<uint16_t>(slotBase + i);
        // Skip slot 0: reserved for system PINs, managed separately
        if (slot == 0) continue;

        cdc::hal::ISecureElement::RMemHeader header = {};
        // ... rest of the loop
```

Or, if slot 0 should be rebuildable in some cases:
```cpp
/**
 * \brief Rebuilds cache for all user slots (excludes system slot 0).
 * \param includeSystemSlot Include system slot 0 in rebuild.
 * \return `true` on success.
 */
bool rebuildVerbose(RebuildLogFn logFn, void* ctx, bool includeSystemSlot = false);
```

Add unit tests:
- Verify slot 0 is skipped during rebuild
- Verify system PINs in slot 0 are not affected by rebuild
- Verify all other slots are properly rebuilt

## References
- TROPIC01 R-Memory allocation documentation
- CWE-690: Unchecked Boundary Condition (implicit assumption)
- CDC Badge slot allocation specification
