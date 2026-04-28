---
title: "[MEDIUM] Duplicate Code: Slot validation logic repeated across methods"
severity: MEDIUM
domain: cdc_core
lens: code-smells
labels:
  - "refactor:consolidate-duplicate-code"
  - "maintainability"
---

## Summary
Slot validation logic is duplicated across multiple methods in `TropicStorage` and `ModuleRegistry`, particularly in `isEntryAllowed`, `isEntryUsed`, and related methods.

**Location:** 
- `components/cdc_core/src/TropicStorage.cpp:434-479`
- `components/cdc_core/src/ModuleRegistry.cpp:752-868`

## Impact
- **Maintenance burden**: Changes to slot validation logic must be applied in multiple places
- **Inconsistency risk**: Bug fixes may be applied to one location but not all
- **Code bloat**: Duplicate code increases binary size

## Evidence
```cpp
// TropicStorage.cpp:468-473 - Duplicate validation logic
bool TropicStorage::isEntryUsed(const CacheEntry& entry) const {
    return (entry.flags & FLAG_USED) != 0;
}

bool TropicStorage::isEntryAllowed(uint16_t slot, uint8_t moduleId) const {
    return TropicSlotMap::instance().isRmemAllowedForModuleId(slot, moduleId);
}

// Similar logic repeated in TropicStorage.cpp:240-252
if (!isEntryAllowed(slot, header.moduleId)) {
    if (logFn) logFn(slot, "mismatched module", ctx);
    continue;
}

CacheEntry& entry = chunk[i];
entry.moduleId = header.moduleId;
entry.flags = static_cast<uint8_t>(header.flags | FLAG_USED);
strncpy(entry.name, header.name, sizeof(entry.name) - 1);
```

```cpp
// ModuleRegistry.cpp:788-868 - Similar validation pattern repeated
bool ModuleRegistry::validateEccRange(const char* mapName, const char* moduleName,
                                      uint16_t minSlots, IModule::SlotRange& range,
                                      uint8_t& moduleId) {
    const auto& slotMap = TropicSlotMap::instance();
    TropicSlotMap::SlotRange ecc = {};

    if (!slotMap.getRangeByName(mapName, TropicSlotMap::SlotType::ECC, &ecc)) {
        char msg[96];
        buildSlotErrorMessage(msg, sizeof(msg), "missing ECC slot map", mapName);
        reportModuleError(moduleName, msg);
        return false;
    }

    uint16_t count = static_cast<uint16_t>(ecc.end - ecc.start + 1);
    if (count < minSlots) {
        char msg[96];
        buildSlotErrorMessage(msg, sizeof(msg), "not enough ECC slots", mapName);
        reportModuleError(moduleName, msg);
        return false;
    }
    // ... similar pattern repeated for RMEM
}
```

## Recommended Fix
Create a centralized slot validation utility class:

```cpp
class SlotValidator {
public:
    static bool validateEccRange(const TropicSlotMap& slotMap,
                                  const char* mapName,
                                  uint16_t minSlots,
                                  std::string& errorMessage);
    
    static bool validateRmemRange(const TropicSlotMap& slotMap,
                                   const char* mapName,
                                   uint16_t minSlots,
                                   uint8_t expectedModuleId,
                                   std::string& errorMessage);
    
    static bool isEntryAllowed(uint16_t slot, uint8_t moduleId);
    static bool isEntryUsed(const CacheEntry& entry);
};

// Usage:
if (!SlotValidator::isEntryUsed(entry)) { /* ... */ }
if (!SlotValidator::isEntryAllowed(slot, moduleId)) { /* ... */ }
```

## References
- DRY (Don't Repeat Yourself) principle - The Pragmatic Programmers
- Martin Fowler, "Refactoring: Improving the Design of Existing Code" - Extract Method, Pull Up Method patterns
