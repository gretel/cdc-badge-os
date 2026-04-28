---
title: "[MEDIUM] Duplicated slot-management logic in TotpStore and PasswordStore"
severity: MEDIUM
domain: modularity
lens: modularity
labels:
  - "audit:maintainability/modularity"
---

## Summary
The `TotpStore` and `PasswordStore` classes in the `mod_totp` and `mod_password` modules respectively contain nearly identical slot-management implementations that should be extracted into a shared base class or utility module.

**Locations:**
- `components/mod_totp/src/TotpStore.cpp`: lines 100-160
- `components/mod_password/src/PasswordStore.cpp`: lines 60-120

## Impact
**Maintenance Burden:**
- Any bug fix or improvement to slot management must be applied in two places
- Risk of divergence over time as modules evolve independently
- Code duplication increases overall codebase size without adding value

**Consistency:**
- Slight variations in implementation may lead to different behaviors
- Makes it harder to ensure uniform behavior across modules

## Evidence
The following methods are nearly identical in both classes:

**`setSlotRange()` implementation:**
```cpp
// TotpStore.cpp:100-112
void TotpStore::setSlotRange(uint16_t start, uint16_t end, uint8_t moduleId) {
    if (start > end || start == 0 || end == 0) {
        hasSlotRange_ = false;
        rmemStart_ = 0;
        rmemEnd_ = 0;
        moduleId_ = 0;
        return;
    }
    hasSlotRange_ = true;
    rmemStart_ = start;
    rmemEnd_ = end;
    moduleId_ = moduleId;
}

// PasswordStore.cpp:60-72 (virtually identical)
void PasswordStore::setSlotRange(uint16_t start, uint16_t end, uint8_t moduleId) {
    if (start > end || start == 0 || end == 0) {
        hasSlotRange_ = false;
        rmemStart_ = 0;
        rmemEnd_ = 0;
        moduleId_ = 0;
        return;
    }
    hasSlotRange_ = true;
    rmemStart_ = start;
    rmemEnd_ = end;
    moduleId_ = moduleId;
}
```

**`capacity()` implementation:**
```cpp
// TotpStore.cpp:114
uint16_t TotpStore::capacity() const {
    if (!hasSlotRange_) return 0;
    return static_cast<uint16_t>(rmemEnd_ - rmemStart_ + 1);
}

// PasswordStore.cpp:78 (virtually identical)
uint16_t PasswordStore::capacity() const {
    if (!hasSlotRange_) return 0;
    return static_cast<uint16_t>(rmemEnd_ - rmemStart_ + 1);
}
```

**`toPhysicalSlot()` implementation:**
```cpp
// TotpStore.cpp:126-132
bool TotpStore::toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const {
    if (!slotOut) return false;
    if (!hasSlotRange_) return false;
    uint32_t slot = static_cast<uint32_t>(rmemStart_) + logicalIndex;
    if (slot > rmemEnd_) return false;
    *slotOut = static_cast<uint16_t>(slot);
    return true;
}

// PasswordStore.cpp:90-96 (virtually identical)
bool PasswordStore::toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const {
    if (!slotOut) return false;
    if (!hasSlotRange_) return false;
    uint32_t slot = static_cast<uint32_t>(rmemStart_) + logicalIndex;
    if (slot > rmemEnd_) return false;
    *slotOut = static_cast<uint16_t>(slot);
    return true;
}
```

**Member variables:**
Both classes have identical private members:
```cpp
// TotpStore.h:58-61
bool hasSlotRange_ = false;
uint16_t rmemStart_ = 0;
uint16_t rmemEnd_ = 0;
uint8_t moduleId_ = 0;

// PasswordStore.h:62-65 (identical)
bool hasSlotRange_ = false;
uint16_t rmemStart_ = 0;
uint16_t rmemEnd_ = 0;
uint8_t moduleId_ = 0;
```

## Recommended Fix
Extract the common slot-management functionality into a shared base class or utility module:

**Option 1: Base class approach**
1. Create `components/cdc_core/include/cdc_core/SlotManager.h` with a base class:
```cpp
namespace cdc::core {

class SlotManager {
public:
    void setSlotRange(uint16_t start, uint16_t end, uint8_t moduleId);
    uint16_t capacity() const;
    bool toPhysicalSlot(uint16_t logicalIndex, uint16_t* slotOut) const;
    bool toLogicalSlot(uint16_t slot, uint16_t* logicalIndexOut) const;
    bool hasSlotRange() const;
    uint8_t moduleId() const;
    uint16_t rmemStart() const;
    uint16_t rmemEnd() const;

protected:
    SlotManager();
    virtual ~SlotManager();

private:
    bool hasSlotRange_ = false;
    uint16_t rmemStart_ = 0;
    uint16_t rmemEnd_ = 0;
    uint8_t moduleId_ = 0;
};

} // namespace cdc::core
```

2. Make `TotpStore` and `PasswordStore` inherit from `SlotManager`:
```cpp
class TotpStore : public core::SlotManager {
    // ... rest of class
};

class PasswordStore : public core::SlotManager {
    // ... rest of class
};
```

3. Remove duplicated methods and members from both classes.

**Option 2: Utility class approach**
1. Create `components/cdc_core/include/cdc_core/SlotMap.h` with a standalone utility class
2. Both `TotpStore` and `PasswordStore` compose a `SlotMap` instance
3. Delegate slot-management calls to the `SlotMap` instance

## References
- DRY principle (Don't Repeat Yourself): https://en.wikipedia.org/wiki/Don%27t_repeat_yourself
- Template Method pattern for shared behavior: https://en.wikipedia.org/wiki/Template_method_pattern
- Existing project pattern: `cdc_core` component already contains shared infrastructure (see `components/cdc_core/`)
