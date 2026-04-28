---
title: "[MEDIUM] Long Parameter List: rmemWriteWithHeader() takes 6 parameters"
severity: MEDIUM
domain: hal
lens: code-smells
labels:
  - "long-parameter-list"
  - "cdc_hal"
---

## Summary
In `components/cdc_hal/include/cdc_hal/ISecureElement.h`, the `rmemWriteWithHeader()` method takes 6 parameters, making it hard to use correctly and requiring changes if the signature needs to evolve.

## Impact
**Error-prone**: Easy to mix up parameter order.

**Hard to read**: Call sites are cluttered with parameters.

**Fragile**: Adding a new parameter requires updating all call sites.

## Evidence
Based on examining the usage in `TotpStore.cpp` and `PasswordStore.cpp`:

`TotpStore.cpp:272-282`:
```cpp
auto res = se->rmemWriteWithHeader(
    slot,
    moduleId_,
    name,
    0,  // flags?
    reinterpret_cast<const uint8_t*>(&payload),
    sizeof(payload)
);
```

`PasswordStore.cpp:230-240`:
```cpp
auto res = se->rmemWriteWithHeader(
    slot,
    moduleId_,
    headerName,
    0,  // flags?
    reinterpret_cast<const uint8_t*>(&payload),
    sizeof(payload)
);
```

The signature is:
```cpp
SeResult rmemWriteWithHeader(uint16_t slot, uint8_t moduleId, const char* name,
                             uint8_t flags, const uint8_t* data, uint16_t len);
```

6 parameters including a mysterious `flags` parameter that's always 0.

## Recommended Fix
1. **Use builder pattern**:
```cpp
struct RMemWriteRequest {
    uint16_t slot;
    uint8_t moduleId;
    const char* name;
    uint8_t flags;
    const uint8_t* data;
    uint16_t len;
};

SeResult rmemWriteWithHeader(const RMemWriteRequest& request);
```

2. **Or use method chaining**:
```cpp
class RMemWriter {
public:
    RMemWriter& slot(uint16_t s) { slot_ = s; return *this; }
    RMemWriter& moduleId(uint8_t m) { moduleId_ = m; return *this; }
    RMemWriter& name(const char* n) { name_ = n; return *this; }
    RMemWriter& flags(uint8_t f) { flags_ = f; return *this; }
    RMemWriter& data(const uint8_t* d, uint16_t l) { data_ = d; len_ = l; return *this; }
    SeResult write();
};
```

**Estimated effort**: ~1 hour to refactor the interface and update call sites.

## References
- Refactoring.com: "Long Parameter List" - https://refactoring.com/catalog/introduceParameterObject
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 7
