---
title: "[LOW] Inconsistent struct member naming: camelCase variations"
severity: LOW
domain: cdc_core
lens: naming-conventions
labels:
  - "audit:code-quality/naming"
---

## Summary
Struct members in the codebase use inconsistent naming patterns. Some use camelCase with abbreviations (`iconDisabled`, `userData`), while others use clearer names or different patterns (`hasError`, `moduleId`).

**Evidence**:
```cpp
// ListView.h - uses camelCase with abbreviations
struct ListItem {
    const char* label;
    uint8_t icon = 0;
    bool iconDisabled = false;  // Abbreviation "icon" + camelCase
    void* userData = nullptr;   // Abbreviation "data"
};

// ModuleRegistry.h - uses clearer names
struct ModuleError {
    bool hasError = false;      // Prefix "has" for boolean
    char message[96] = {0};
};

// IModule.h - uses mixed patterns
struct SlotRange {
    bool hasEcc = false;
    bool hasRmem = false;
    uint8_t eccStart = 0;
    uint8_t eccEnd = 0;
    uint16_t rmemStart = 0;
    uint16_t rmemEnd = 0;
    uint8_t moduleId = 0;       // Abbreviation "Id"
};
```

## Impact
- **Readability**: Abbreviations like `userData`, `iconDisabled` are less clear than full names
- **Consistency**: Boolean naming is inconsistent (`hasError` vs `iconDisabled`)
- **Maintainability**: Developers may be unsure which pattern to follow

## Evidence
- File: `components/cdc_views/include/cdc_views/ListView.h`
- File: `components/cdc_core/include/cdc_core/ModuleRegistry.h`
- File: `components/cdc_core/include/cdc_core/IModule.h`
- Inconsistent patterns: `userData`, `iconDisabled`, `moduleId`, `hasError`

## Recommended Fix
Use consistent, clear naming:
```cpp
// Consistent naming for struct members
struct ListItem {
    const char* label;
    uint8_t iconIndex = 0;
    bool iconIsDisabled = false;  // Use "is" prefix for booleans
    void* data = nullptr;          // Or "userData" consistently
};

struct ModuleError {
    bool hasError = false;
    char message[96] = {0};
};

struct SlotRange {
    bool hasEcc = false;
    bool hasRmem = false;
    uint8_t eccStart = 0;
    uint8_t eccEnd = 0;
    uint16_t rmemStart = 0;
    uint16_t rmemEnd = 0;
    uint8_t moduleIndex = 0;      // Use "index" instead of "Id"
};
```

## References
- C++ Core Guidelines: [C.35: Use a consistent naming style](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#C35)
- Google C++ Style Guide: [Boolean names](https://google.github.io/styleguide/cppguide.html#Boolean_Names) - use "is", "has", "are" prefixes
