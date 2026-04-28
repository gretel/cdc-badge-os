---
title: "[007] [MEDIUM] Mixed abstraction levels in Tropic01Element::rmemWriteWithHeader"
severity: MEDIUM
domain: code-readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
In `components/cdc_hal/src/Tropic01Element.cpp:683-725`, the `rmemWriteWithHeader` function mixes high-level API concerns (metadata packaging) with low-level implementation details (buffer allocation, byte-by-byte copying):

```cpp
SeResult Tropic01Element::rmemWriteWithHeader(uint16_t slot, uint8_t moduleId,
                                              const char* name, uint8_t flags,
                                              const uint8_t* payload, uint16_t payloadLen) {
    if (slot >= RMEM_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;
    }
    if (payloadLen > (RMEM_SLOT_SIZE - sizeof(RMemHeader))) {
        return SeResult::INVALID_PARAM;
    }

    // R-Memory requires erase before write
    SeResult eraseRes = rmemErase(slot);
    if (eraseRes != SeResult::OK && eraseRes != SeResult::SLOT_EMPTY) {
        return eraseRes;
    }

    RMemHeader header = {};
    header.magic = RMEM_HEADER_MAGIC;
    header.moduleId = moduleId;
    header.flags = flags;
    header.payloadLen = payloadLen;
    if (name) {
        strncpy(header.name, name, sizeof(header.name) - 1);
        header.name[sizeof(header.name) - 1] = '\0';
    }
    header.checksum = computeHeaderChecksum(header);

    uint8_t buffer[RMEM_SLOT_SIZE] = {};
    memcpy(buffer, &header, sizeof(header));
    if (payloadLen > 0 && payload) {
        memcpy(buffer + sizeof(header), payload, payloadLen);
    }

    return rmemWrite(slot, buffer, static_cast<uint16_t>(sizeof(header) + payloadLen));
}
```

## Impact
- **Cognitive switching**: Reader must understand both the metadata structure AND the byte-level buffer manipulation
- **Testing difficulty**: Hard to unit-test the header packaging logic separately from the I/O
- **Refactoring risk**: Changes to buffer layout require understanding both high-level and low-level concerns

## Evidence
**File**: `components/cdc_hal/src/Tropic01Element.cpp:683-725`

The function performs three distinct operations:
1. Parameter validation (high-level)
2. Header construction with checksum (metadata layer)
3. Buffer assembly and write (low-level I/O)

## Recommended Fix
Extract the header construction into a separate private method:

```cpp
// Private helper
uint16_t Tropic01Element::packHeaderWithPayload(uint8_t* buffer, uint16_t bufferSize,
                                                 uint8_t moduleId, const char* name,
                                                 uint8_t flags, const uint8_t* payload,
                                                 uint16_t payloadLen) {
    RMemHeader header = {};
    header.magic = RMEM_HEADER_MAGIC;
    header.moduleId = moduleId;
    header.flags = flags;
    header.payloadLen = payloadLen;
    if (name) {
        strncpy(header.name, name, sizeof(header.name) - 1);
        header.name[sizeof(header.name) - 1] = '\0';
    }
    header.checksum = computeHeaderChecksum(header);

    memcpy(buffer, &header, sizeof(header));
    if (payloadLen > 0 && payload) {
        memcpy(buffer + sizeof(header), payload, payloadLen);
    }
    return static_cast<uint16_t>(sizeof(header) + payloadLen);
}

// Simplified public method
SeResult Tropic01Element::rmemWriteWithHeader(uint16_t slot, uint8_t moduleId,
                                              const char* name, uint8_t flags,
                                              const uint8_t* payload, uint16_t payloadLen) {
    if (slot >= RMEM_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;
    }
    if (payloadLen > (RMEM_SLOT_SIZE - sizeof(RMemHeader))) {
        return SeResult::INVALID_PARAM;
    }

    SeResult eraseRes = rmemErase(slot);
    if (eraseRes != SeResult::OK && eraseRes != SeResult::SLOT_EMPTY) {
        return eraseRes;
    }

    uint8_t buffer[RMEM_SLOT_SIZE] = {};
    uint16_t totalLen = packHeaderWithPayload(buffer, bufferSize, moduleId, name, flags, payload, payloadLen);
    return rmemWrite(slot, buffer, totalLen);
}
```

## References
- [Clean Architecture - Robert C. Martin](https://www.amazon.com/Clean-Architecture-Craftsmans-Software-Structure/dp/0134494164)
- [Single Responsibility Principle](https://en.wikipedia.org/wiki/Single-responsibility_principle)
