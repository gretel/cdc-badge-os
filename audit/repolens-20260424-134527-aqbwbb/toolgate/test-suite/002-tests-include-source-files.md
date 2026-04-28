---
title: "[MEDIUM] Tests include .cpp source files directly instead of linking component"
severity: MEDIUM
domain: test-suite
lens: toolgate/test-suite
labels:
  - "audit:toolgate/test-suite"
---

## Summary
Two test files directly include source files using `#include "src/vcard_store.cpp"` and `#include "src/ble_vcard.cpp"` instead of linking against the `mod_vcard` component:

- `test/test_vcard_store/test_vcard_store.cpp` line 2: `#include "../../components/mod_vcard/src/vcard_store.cpp"`
- `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp` line 2: `#include "../../components/mod_vcard/src/ble_vcard.cpp"`

This is a common anti-pattern that causes:
1. Symbol duplication when component is also linked
2. Build order dependencies
3. Tight coupling between tests and implementation

## Impact
- **Symbol collisions** - If the component is linked via CMake, functions may be defined twice
- **Fragile tests** - Tests break when source file locations change
- **Slower builds** - Source files are recompiled with each test build
- **Non-standard practice** - ESP-IDF expects components to be linked via `REQUIRES`

## Evidence
From `test/test_vcard_store/test_vcard_store.cpp`:
```cpp
#include "mod_vcard/vcard_store.h"
#include "../../components/mod_vcard/src/vcard_store.cpp"  // BAD
#include <cstring>

void test_vcard_validate() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));
}
```

From `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp`:
```cpp
#include "mod_vcard/ble_vcard.h"
#include "../../components/mod_vcard/src/ble_vcard.cpp"  // BAD

void test_ble_vcard_symbols() {
    ble_vcard_init();
    ble_vcard_set_exchange_enabled(true);
}
```

## Recommended Fix
Remove the direct `#include` of `.cpp` files and link the component via CMake:

**Step 1:** Remove the include line from each test file:
```cpp
// BEFORE
#include "mod_vcard/vcard_store.h"
#include "../../components/mod_vcard/src/vcard_store.cpp"
#include <cstring>

// AFTER
#include "mod_vcard/vcard_store.h"
#include <cstring>
```

**Step 2:** Ensure CMakeLists.txt includes `mod_vcard` in REQUIRES:
```cmake
idf_component_register(
    SRCS "test_vcard_store.cpp"
    INCLUDE_DIRS "." "../../components/mod_vcard/include"
    REQUIRES
        mod_vcard    # <-- Links the component properly
        cdc_log
        nvs_flash
)
```

## References
- [ESP-IDF Component Architecture](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/kbuild.html#id12)
- [CMake target_link_libraries](https://cmake.org/cmake/help/latest/command/target_link_libraries.html)
