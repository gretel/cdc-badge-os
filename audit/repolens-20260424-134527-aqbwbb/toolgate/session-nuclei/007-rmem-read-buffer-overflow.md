---
title: "[MEDIUM] Potential buffer overflow in rmemReadWithHeader with large payloadLen"
severity: MEDIUM
domain: cdc-badge-os
lens: library/cdc-badge-os
labels:
  - "audit:toolgate/session-nuclei"
---

## Summary
The `rmemReadWithHeader` function in `components/cdc_hal/src/Tropic01Element.cpp` has a potential buffer overflow vulnerability when reading R-Memory slots with large `payloadLen` values stored in the header. The function validates `header.payloadLen` against `RMEM_SLOT_SIZE - sizeof(RMemHeader)` but then uses this value directly in a `memcpy` without re-validating against the actual data read from the secure element.

**Location**: `components/cdc_hal/src/Tropic01Element.cpp:730-777`

## Impact
- **Stack Buffer Overflow**: If a malicious or corrupted R-Memory slot contains a `payloadLen` larger than the actual data read, the `memcpy` at line 770 could overflow the `payloadOut` buffer
- **Memory Corruption**: Depending on stack layout, this could overwrite return addresses or local variables
- **Secure Element Data Trust**: The function trusts the header's `payloadLen` without cross-referencing with actual bytes read

## Evidence
```cpp
// components/cdc_hal/src/Tropic01Element.cpp:730-777
SeResult Tropic01Element::rmemReadWithHeader(uint16_t slot, RMemHeader* headerOut,
                                             uint8_t* payloadOut, uint16_t payloadMax,
                                             uint16_t* payloadLenOut) {
    if (slot >= RMEM_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;
    }

    uint8_t buffer[RMEM_SLOT_SIZE] = {};  // 476 bytes stack buffer
    uint16_t actualLen = 0;
    SeResult res = rmemRead(slot, buffer, sizeof(buffer), &actualLen);
    if (res != SeResult::OK) {
        return res;
    }
    if (actualLen < sizeof(RMemHeader)) {
        return SeResult::ERROR;
    }

    RMemHeader header = {};
    memcpy(&header, buffer, sizeof(header));
    if (!validateHeader(header)) {
        return SeResult::ERROR;
    }
    if (header.payloadLen > (RMEM_SLOT_SIZE - sizeof(RMemHeader))) {
        return SeResult::ERROR;
    }
    if (actualLen < static_cast<uint16_t>(sizeof(RMemHeader) + header.payloadLen)) {  // Check at 754
        return SeResult::ERROR;
    }

    if (headerOut) {
        *headerOut = header;
    }
    if (payloadLenOut) {
        *payloadLenOut = header.payloadLen;  // Stores unvalidated length
    }
    if (payloadOut && header.payloadLen > 0) {
        if (payloadMax < header.payloadLen) {
            return SeResult::INVALID_PARAM;
        }
        memcpy(payloadOut, buffer + sizeof(RMemHeader), header.payloadLen);  // Line 770
    }

    return SeResult::OK;
}
```

The vulnerability exists because:
1. Line 754 checks if `actualLen >= sizeof(RMemHeader) + header.payloadLen`
2. But `actualLen` comes from the secure element which could be corrupted or manipulated
3. If an attacker can write to R-Memory (via serial commands), they could craft a header with inflated `payloadLen`

## Recommended Fix
Add an additional validation to ensure `header.payloadLen` doesn't exceed `actualLen - sizeof(RMemHeader)`:

```cpp
SeResult Tropic01Element::rmemReadWithHeader(uint16_t slot, RMemHeader* headerOut,
                                             uint8_t* payloadOut, uint16_t payloadMax,
                                             uint16_t* payloadLenOut) {
    // ... existing validation ...

    if (!validateHeader(header)) {
        return SeResult::ERROR;
    }
    if (header.payloadLen > (RMEM_SLOT_SIZE - sizeof(RMemHeader))) {
        return SeResult::ERROR;
    }
    
    // Add validation: payloadLen must not exceed actual data read
    uint16_t expectedLen = static_cast<uint16_t>(sizeof(RMemHeader) + header.payloadLen);
    if (actualLen < expectedLen) {
        return SeResult::ERROR;
    }
    
    // Additional safety: cap payloadLen to actual available data
    uint16_t actualPayloadLen = actualLen - sizeof(RMemHeader);
    if (header.payloadLen > actualPayloadLen) {
        LOG_W(TAG, "Header payloadLen (%u) exceeds actual data (%u)", 
              header.payloadLen, actualPayloadLen);
        header.payloadLen = actualPayloadLen;  // Cap to safe value
    }

    if (headerOut) {
        *headerOut = header;
    }
    if (payloadLenOut) {
        *payloadLenOut = header.payloadLen;
    }
    if (payloadOut && header.payloadLen > 0) {
        if (payloadMax < header.payloadLen) {
            return SeResult::INVALID_PARAM;
        }
        memcpy(payloadOut, buffer + sizeof(RMemHeader), header.payloadLen);
    }

    return SeResult::OK;
}
```

## References
- `components/cdc_hal/src/Tropic01Element.cpp:730-777` - Function implementation
- `components/cdc_hal/include/cdc_hal/ISecureElement.h:170-180` - Interface definition
- CWE-120: Buffer copy without checking size of input
- CWE-125: Out-of-bounds read
