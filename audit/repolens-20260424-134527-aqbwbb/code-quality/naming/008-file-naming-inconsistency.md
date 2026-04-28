---
title: "[LOW] File naming inconsistency: camelCase vs PascalCase for class files"
severity: LOW
domain: components
lens: naming-conventions
labels:
  - "audit:code-quality/naming"
---

## Summary
The codebase uses inconsistent file naming conventions for C++ class files. Some use camelCase (`tropic_slot_map.h`), while others use PascalCase (`TotpModule.h`, `ServiceRegistry.h`).

**Evidence**:
```
// camelCase file names
main/tropic_slot_map.h
components/cdc_log/src/cdc_log.cpp

// PascalCase file names
components/mod_totp/include/mod_totp/TotpModule.h
components/mod_totp/src/TotpModule.cpp
components/cdc_core/include/cdc_core/ServiceRegistry.h
components/cdc_core/src/ServiceRegistry.cpp
components/cdc_views/include/cdc_views/ListView.h
components/cdc_views/src/ListView.cpp
```

## Impact
- **Consistency**: No single standard for file naming
- **Discoverability**: Developers may not know whether to look for `tropic_slot_map.h` or `TropicSlotMap.h`
- **Maintainability**: Unclear which convention to follow for new files

## Evidence
- File: `main/tropic_slot_map.h` (camelCase)
- File: `components/mod_totp/include/mod_totp/TotpModule.h` (PascalCase)
- File: `components/cdc_core/include/cdc_core/ServiceRegistry.h` (PascalCase)
- File: `components/cdc_log/src/cdc_log.cpp` (camelCase)

## Recommended Fix
Choose one convention and apply consistently:

**Option 1: Use PascalCase for class files (recommended for C++)**
```
main/TropicSlotMap.h
components/cdc_log/src/CdcLog.cpp
```

**Option 2: Use camelCase for all files**
```
components/mod_totp/include/mod_totp/totpModule.h
components/cdc_core/include/cdc_core/serviceRegistry.h
```

Given that most of the codebase uses PascalCase for class files, migrate the camelCase files to PascalCase.

## References
- C++ Core Guidelines: [C.12: Use UpperCamelCase for class names](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#C12)
- Google C++ Style Guide: [File names](https://google.github.io/styleguide/cppguide.html#File_Names) - uses lowercase with underscores
- ESP-IDF convention: Uses camelCase for most files
