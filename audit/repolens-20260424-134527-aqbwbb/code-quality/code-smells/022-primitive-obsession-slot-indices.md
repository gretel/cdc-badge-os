---
title: "[MEDIUM] Primitive Obsession: Slot indices and module IDs use raw integers"
severity: MEDIUM
domain: cdc_core
lens: code-smells
labels:
  - "refactor:introduce-value-object"
  - "type-safety"
---

## Summary
The codebase uses raw integers (`uint8_t`, `uint16_t`) for slot indices, module IDs, and other domain-specific values without type safety. This makes it easy to mix up parameters and reduces code clarity.

**Location:** Throughout `cdc_core` and `cdc_hal` components

## Impact
- **Type safety**: Easy to accidentally pass slot index where module ID is expected (or vice versa)
- **Self-documentation**: Raw integers don't convey domain meaning
- **Validation**: No compile-time guarantees about valid ranges
- **Refactoring**: Harder to track where specific numeric types are used

## Evidence
```cpp
// IModule.h:54-69 - SlotRequest uses raw integers
struct SlotRequest {
    const char* mapName = nullptr;
    uint8_t minEccSlots = 0;
    uint16_t minRmemSlots = 0;
};

struct SlotRange {
    bool hasEcc = false;
    bool hasRmem = false;
    uint8_t eccStart = 0;
    uint8_t eccEnd = 0;
    uint16_t rmemStart = 0;
    uint16_t rmemEnd = 0;
    uint8_t moduleId = 0;  // What does this mean? Is it valid?
};

// TropicStorage.cpp:160-175 - Raw slot and moduleId parameters
bool TropicStorage::writeSlot(uint8_t moduleId, uint16_t slot, const char* name, uint8_t flags) {
    if (!isEntryAllowed(slot, moduleId)) {
        return false;
    }

    CacheEntry entry = {};
    entry.moduleId = moduleId;  // Direct assignment, no validation
    entry.flags = static_cast<uint8_t>(flags | FLAG_USED);
    // ...
}

// Tropic01Element.cpp:330-350 - Raw slot parameter
SeResult Tropic01Element::eccGenerate(uint8_t slot, EccCurve curve) {
    if (slot >= ECC_SLOT_COUNT) {
        return SeResult::INVALID_PARAM;
    }
    // ...
}
```

## Recommended Fix
Introduce value objects for domain-specific types:

```cpp
// SlotId.h
namespace cdc::core {

/**
 * \brief Strongly-typed slot identifier.
 */
struct SlotId {
    uint16_t value;
    bool isValid() const { return value > 0 && value < 512; }
};

/**
 * \brief Module identifier with validation.
 */
struct ModuleId {
    uint8_t value;
    bool isValid() const { return value > 0 && value < 255; }
    static constexpr ModuleId Invalid() { return {0}; }
};

/**
 * \brief Slot range with bounds checking.
 */
struct SlotRange {
    SlotId start;
    SlotId end;
    bool contains(SlotId slot) const;
    uint16_t size() const;
};

} // namespace cdc::core
```

Usage:
```cpp
bool TropicStorage::writeSlot(ModuleId moduleId, SlotId slot, const char* name, uint8_t flags) {
    if (!moduleId.isValid()) return false;
    if (!slot.isValid()) return false;
    if (!isEntryAllowed(slot.value, moduleId.value)) {
        return false;
    }
    // ...
}
```

## References
- Martin Fowler, "Refactoring: Improving the Design of Existing Code" - Replace Data Value with Object
- Clean Code by Robert C. Martin: Use meaningful types to make code self-documenting
