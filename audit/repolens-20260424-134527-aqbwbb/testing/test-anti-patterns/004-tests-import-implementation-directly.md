---
title: "[MEDIUM] Test files import implementation files directly instead of headers"
severity: MEDIUM
domain: testing
lens: test-anti-patterns
labels:
  - "audit:testing/test-anti-patterns"
---

## Summary
Three test files in the `test/` directory import C++ implementation files (`.cpp`) directly instead of using header files. This is a common anti-pattern that can lead to multiple definition errors, tighter coupling, and tests that don't properly test the public interface.

**Affected files:**
- `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp:2`
- `test/test_vcard_store/test_vcard_store.cpp:2`

## Impact
1. **Multiple definition errors**: If the same `.cpp` file is included in multiple test files or linked with the main build, symbols may be defined twice.
2. **Tests bypass public API**: Including `.cpp` files directly may expose internal functions that aren't part of the public interface.
3. **Build fragility**: Changes to include paths or build configuration may break tests silently.
4. **No compilation unit isolation**: Each test file gets its own copy of the included implementation, potentially with different macro definitions.

## Evidence
**`test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp:1-3`:**
```cpp
#include "mod_vcard/ble_vcard.h"
#include "../../components/mod_vcard/src/ble_vcard.cpp"  // Direct .cpp include

/**
 * \brief Link/symbol smoke test for BLE vCard API.
 * \return void
 */
void test_ble_vcard_symbols() {
    ble_vcard_init();
    ble_vcard_set_exchange_enabled(true);
}
```

**`test/test_vcard_store/test_vcard_store.cpp:1-3`:**
```cpp
#include "mod_vcard/vcard_store.h"
#include "../../components/mod_vcard/src/vcard_store.cpp"  // Direct .cpp include
#include <cstring>

/**
 * \brief Smoke-test for vCard validation/store path.
 * \return void
 */
void test_vcard_validate() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));
}
```

**`test/test_vcard_module_link/test_vcard_module_link.cpp:3`:**
```cpp
#include "mod_vcard/VcardModule.h"

extern "C" void mod_vcard_register();  // Declared but not defined in header

void test_vcard_module_link() {
    mod_vcard_register();  // Relies on external linkage
}
```

## Recommended Fix
1. **Include only headers** and ensure the build system links the implementation:
```cpp
// test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp
#include "mod_vcard/ble_vcard.h"
// Remove: #include "../../components/mod_vcard/src/ble_vcard.cpp"
```

2. **Update CMakeLists.txt** to include test sources in the build with proper dependencies:
```cmake
idf_component_register(
    SRCS "test_ble_vcard_symbols.cpp"
    INCLUDE_DIRS "."
    REQUIRES mod_vcard  # Link the component, don't include .cpp
)
```

3. **For `test_vcard_module_link`**, ensure `mod_vcard_register()` is declared in a header:
```cpp
// components/mod_vcard/include/mod_vcard/VcardModule.h
#ifdef __cplusplus
extern "C" {
#endif
void mod_vcard_register();
#ifdef __cplusplus
}
#endif
```

## References
- [Including .cpp files: When and why](https://stackoverflow.com/questions/495021/why-can-i-include-a-cpp-file-in-another-cpp-file)
- [Test isolation and public API](https://martinfowler.com/bliki/TestSmell.html#HiddenDependencies)
