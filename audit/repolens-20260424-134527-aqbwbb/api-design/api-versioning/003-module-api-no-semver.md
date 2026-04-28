---
title: "[LOW] Module Version Interface Returns String Without Semantic Versioning"
severity: LOW
domain: API Design
lens: api-versioning
labels:
  - "audit:api-design/api-versioning"
---

## Summary
The `IModule::getVersion()` interface (`components/cdc_core/include/cdc_core/IModule.h:71`) returns a generic string without enforcing semantic versioning format. This makes programmatic version comparison difficult.

**Evidence:**
- `components/cdc_core/include/cdc_core/IModule.h:71-73` - Virtual method returns `const char*` with no format specification
- `components/cdc_core/src/ModuleRegistry.cpp` - Version logged but not parsed/compared

## Impact
- Modules cannot programmatically check compatibility (e.g., "needs TOTP module >= 1.2.0")
- No distinction between major/minor/patch changes
- Hard to track breaking changes across modules

## Evidence
Current interface:
```cpp
/**
 * Get module version string
 */
virtual const char* getVersion() const = 0;
```

No format enforced, no version comparison helper.

## Recommended Fix
Add semantic versioning support:

1. Add version struct:
```cpp
struct ModuleVersion {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    
    bool operator==(const ModuleVersion& other) const;
    bool operator<(const ModuleVersion& other) const;
    bool isCompatibleWith(const ModuleVersion& other) const;  // major match
};
```

2. Update interface:
```cpp
virtual ModuleVersion getVersion() const;
virtual const char* getVersionString() const;  // For display
```

3. Add version requirement to `SlotRequest`:
```cpp
struct SlotRequest {
    const char* mapName = nullptr;
    uint8_t minEccSlots = 0;
    uint16_t minRmemSlots = 0;
    ModuleVersion minVersion = {1, 0, 0};  // NEW
};
```

## References
- Semantic Versioning: https://semver.org/
- ESP-IDF Version Scheme: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html#config-esp-idf-version
