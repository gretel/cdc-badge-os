---
title: "[MEDIUM] Tests directly include source files instead of linking to components"
severity: MEDIUM
domain: test-maintainability
lens: test-maintainability
labels:
  - "audit:testing/test-maintainability"
---

## Summary
Tests in `/input/20260423-132359-oj8ayc/cdc-badge-os/test/` directly include `.cpp` source files instead of linking to compiled components:

**test_vcard_store.cpp:2**
```cpp
#include "../../components/mod_vcard/src/vcard_store.cpp"
```

**test_ble_vcard_symbols.cpp:2**
```cpp
#include "../../components/mod_vcard/src/ble_vcard.cpp"
```

This creates tight coupling between tests and implementation details.

## Impact
- **Brittle tests**: Moving or renaming source files breaks tests
- **Duplicate compilation**: Source files are recompiled for each test
- **Hard to maintain**: Relative paths break when restructuring directories
- **No encapsulation**: Tests access internal implementation directly

## Evidence
- `test/test_vcard_store/test_vcard_store.cpp:2` - `#include "../../components/mod_vcard/src/vcard_store.cpp"`
- `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp:2` - `#include "../../components/mod_vcard/src/ble_vcard.cpp"`

## Recommended Fix
1. Create proper CMakeLists.txt for test components
2. Link tests to compiled components instead of including source files

Example CMakeLists.txt for test:
```cmake
idf_component_register(
    SRCS "test_vcard_store.cpp"
    INCLUDE_DIRS "."
    REQUIRES mod_vcard  # Link to compiled component
)
```

Then update includes to use header-only:
```cpp
#include "mod_vcard/vcard_store.h"  // No .cpp include
```

## References
- [ESP-IDF Component Structure](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#components)
- [Test Linking Best Practices](https://google.github.io/styleguide/cppguide.html#Namespaces_and_Global_Variables)
