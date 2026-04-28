---
title: "[HIGH] Tests include source files directly instead of headers"
severity: HIGH
domain: testing
lens: test-anti-patterns
labels:
  - "test-structure"
---

## Summary
Three test files directly include source (`.cpp`) files instead of just headers, causing potential compilation issues and testing implementation details rather than public APIs:

1. `test/test_vcard_store/test_vcard_store.cpp:2` - includes `../../components/mod_vcard/src/vcard_store.cpp`
2. `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp:2` - includes `../../components/mod_vcard/src/ble_vcard.cpp`

## Impact
- **Compilation fragility**: Including `.cpp` files can cause multiple definition errors if the same source is included by multiple tests or if the build system also compiles the source file
- **Testing implementation**: Tests become coupled to implementation details rather than stable public interfaces
- **Build order dependency**: Tests may fail to compile if the source file changes and includes different headers
- **Namespace pollution**: All static functions and variables in the source become visible to the test

## Evidence
```cpp
// test/test_vcard_store/test_vcard_store.cpp:2
#include "../../components/mod_vcard/src/vcard_store.cpp"

// test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp:2
#include "../../components/mod_vcard/src/ble_vcard.cpp"
```

## Recommended Fix
1. Remove the `.cpp` includes from test files
2. Include only the corresponding header files:
   - `test_vcard_store.cpp` should include `mod_vcard/vcard_store.h`
   - `test_ble_vcard_symbols.cpp` should include `mod_vcard/ble_vcard.h`
3. Ensure the test CMakeLists.txt links against the `mod_vcard` component so symbols are available

## References
- ESP-IDF Testing Guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/unit-tests.html
- C++ Testing Best Practices: Include headers, not source files (general C++ convention)
