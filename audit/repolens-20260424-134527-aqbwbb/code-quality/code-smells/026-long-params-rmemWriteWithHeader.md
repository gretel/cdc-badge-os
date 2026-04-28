---
title: "[MEDIUM] Long Parameter List: rmemWriteWithHeader has 6+ parameters"
severity: MEDIUM
domain: cdc_hal
lens: code-smells
labels:
  - "refactor:introduce-parameter-object"
  - "readability"
---

## Summary
The `rmemWriteWithHeader` method in `ISecureElement` has 6 parameters, making it difficult to call correctly and understand.

**Location:** `components/cdc_hal/include/cdc_hal/ISecureElement.h` and `components/cdc_hal/src/Tropic01Element.cpp:685-719`

## Evidence
```cpp
// ISecureElement.h - Long parameter list
SeResult rmemWriteWithHeader(uint16_t slot, uint8_t moduleId,
                             const char* name, uint8_t flags,
                             const uint8_t* payload, uint16_t payloadLen) override;

// Tropic01Element.cpp:685-719 - Implementation with many parameters
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
    // ...
}
```

## Impact
- **Readability**: Callers must remember parameter order
- **Error-prone**: Easy to swap `payload` and `payloadLen`
- **Extensibility**: Adding new parameters breaks existing calls
- **Self-documentation**: Parameters don't convey their purpose clearly

## Recommended Fix
Use a parameter object (builder pattern):

```cpp
// ISecureElement.h
struct RMemRecord {
    uint8_t moduleId;
    char name[32];
    uint8_t flags;
    const uint8_t* payload = nullptr;
    uint16_t payloadLen = 0;

    // Builder methods
    RMemRecord& withName(const char* name) {
        strncpy(this->name, name, sizeof(this->name) - 1);
        this->name[sizeof(this->name) - 1] = '\0';
        return *this;
    }

    RMemRecord& withFlags(uint8_t flags) {
        this->flags = flags;
        return *this;
    }

    RMemRecord& withPayload(const uint8_t* data, uint16_t len) {
        this->payload = data;
        this->payloadLen = len;
        return *this;
    }
};

SeResult rmemWriteWithHeader(uint16_t slot, RMemRecord record) override;

// Usage:
se.rmemWriteWithHeader(slot, RMemRecord{}
    .withName("my_record")
    .withFlags(FLAG_USED)
    .withPayload(data, len));
```

Alternative: Use a struct with positional parameters:

```cpp
struct RMemWriteParams {
    uint16_t slot;
    uint8_t moduleId;
    const char* name;
    uint8_t flags;
    const uint8_t* payload;
    uint16_t payloadLen;
};

SeResult rmemWriteWithHeader(RMemWriteParams params) override;

// Usage:
se.rmemWriteWithHeader(RMemWriteParams{
    .slot = 5,
    .moduleId = 2,
    .name = "my_record",
    .flags = FLAG_USED,
    .payload = data,
    .payloadLen = len
});
```

## References
- Martin Fowler, "Refactoring: Improving the Design of Existing Code" - Introduce Parameter Object
- Clean Code by Robert C. Martin: Functions should have few parameters (ideally 3 or less)
