---
title: "[MEDIUM] Inappropriate Intimacy: TropicStorage tightly coupled to TropicSlotMap internals"
severity: MEDIUM
domain: cdc_core
lens: code-smells
labels:
  - "refactor:reduce-coupling"
  - "maintainability"
---

## Summary
`TropicStorage` has deep knowledge of `TropicSlotMap` internals, calling methods directly and accessing internal data structures. This creates tight coupling that makes both classes harder to modify independently.

**Location:** `components/cdc_core/src/TropicStorage.cpp`

## Evidence
```cpp
// TropicStorage.cpp:86-100 - Direct access to TropicSlotMap internals
bool TropicStorage::forEachSlot(uint8_t moduleId, uint16_t fromSlot, uint16_t toSlot,
                                SlotCallback cb, void* ctx) {
    if (!cb) return false;
    if (fromSlot > toSlot) return false;
    auto& slotMap = TropicSlotMap::instance();  // Direct singleton access
    TropicSlotMap::SlotRange range = {};
    if (!slotMap.getRangeByModuleId(moduleId, TropicSlotMap::SlotType::RMEM, &range)) {
        return false;
    }
    if (fromSlot == 0 || toSlot == 0xFFFF) {
        fromSlot = range.start;  // Accessing range struct fields directly
        toSlot = range.end;
    }
    // ...
}

// TropicStorage.cpp:420-438 - Computing signature based on slot map internals
uint32_t TropicStorage::computeMapSignature() const {
    // FNV-1a 32-bit over map constants
    uint32_t hash = 2166136261u;
    auto mix = [&hash](uint32_t v) {
        hash ^= v;
        hash *= 16777619u;
    };

    mix(TropicSlotMap::instance().computeMapSignature());  // Direct call to get signature

    return hash;
}

// TropicStorage.cpp:470-474 - Another direct dependency
bool TropicStorage::isEntryAllowed(uint16_t slot, uint8_t moduleId) const {
    return TropicSlotMap::instance().isRmemAllowedForModuleId(slot, moduleId);
}
```

## Impact
- **Tight coupling**: `TropicStorage` changes when `TropicSlotMap` changes
- **Testing difficulty**: Hard to test `TropicStorage` without `TropicSlotMap`
- **Circular dependencies**: Both classes depend on each other's implementation
- **Maintenance burden**: Changes to slot map logic ripple through storage

## Recommended Fix
Introduce an interface or adapter pattern:

```cpp
// SlotMapAdapter.h - Interface for slot map access
class ISlotMapAdapter {
public:
    virtual ~ISlotMapAdapter() = default;
    virtual bool getRangeByModuleId(uint8_t moduleId, 
                                     SlotType type,
                                     SlotRange* out) const = 0;
    virtual bool isRmemAllowedForModuleId(uint16_t slot, uint8_t moduleId) const = 0;
    virtual uint32_t computeSignature() const = 0;
};

// Wrap TropicSlotMap
class TropicSlotMapAdapter : public ISlotMapAdapter {
public:
    bool getRangeByModuleId(uint8_t moduleId, SlotType type, SlotRange* out) const override {
        return TropicSlotMap::instance().getRangeByModuleId(moduleId, type, out);
    }
    
    bool isRmemAllowedForModuleId(uint16_t slot, uint8_t moduleId) const override {
        return TropicSlotMap::instance().isRmemAllowedForModuleId(slot, moduleId);
    }
    
    uint32_t computeSignature() const override {
        return TropicSlotMap::instance().computeMapSignature();
    }
};

// TropicStorage depends on interface, not concrete class
class TropicStorage {
public:
    void setSlotMapAdapter(ISlotMapAdapter* adapter) { adapter_ = adapter; }
    
private:
    ISlotMapAdapter* adapter_ = nullptr;
    
    bool isEntryAllowed(uint16_t slot, uint8_t moduleId) const {
        return adapter_->isRmemAllowedForModuleId(slot, moduleId);
    }
};
```

## References
- Martin Fowler, "Refactoring: Improving the Design of Existing Code" - Inappropriate Intimacy
- Dependency Inversion Principle (SOLID): Depend on abstractions, not concretions
