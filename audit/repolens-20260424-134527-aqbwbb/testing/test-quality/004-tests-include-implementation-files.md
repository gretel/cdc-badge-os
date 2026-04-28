---
title: "[MEDIUM] Tests include implementation files directly (`.cpp`) instead of headers"
severity: MEDIUM
domain: testing/test-quality
lens: test-structure
labels:
  - "test-quality"
  - "test-structure"
  - "build-system"
---

## Summary

Two test files include implementation files (`.cpp`) directly instead of just headers:

1. **`test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp`**
   ```cpp
   #include "../../components/mod_vcard/src/ble_vcard.cpp"
   ```

2. **`test/test_vcard_store/test_vcard_store.cpp`**
   ```cpp
   #include "../../components/mod_vcard/src/vcard_store.cpp"
   ```

This is an unusual pattern that bypasses normal compilation and linking. It suggests tests are directly embedding the implementation rather than testing the compiled module.

## Impact

**Build system complexity:**
- Tests may compile differently than the actual module (different flags, macros)
- Hard to track dependencies between tests and implementation
- May cause symbol collisions if both test and module are linked together

**Testing gaps:**
- Tests may not reflect actual module behavior when compiled separately
- Interface contracts (header-only API) are not validated
- May hide linking issues that occur in production builds

**Maintenance issues:**
- Relative paths (`../../components/...`) are fragile
- Moving files breaks tests silently (just need to update paths)
- Not clear what the test's dependency graph looks like

**Inconsistency:**
- `test/test_vcard_module_link/test_vcard_module_link.cpp` does NOT include the `.cpp` file
  - It just declares `extern "C" void mod_vcard_register();`
  - This suggests the `.cpp` inclusion pattern is ad-hoc, not intentional

## Evidence

**File: `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp`**
```cpp
#include "mod_vcard/ble_vcard.h"
#include "../../components/mod_vcard/src/ble_vcard.cpp"  // Direct .cpp include!

void test_ble_vcard_symbols() {
    ble_vcard_init();
    ble_vcard_set_exchange_enabled(true);
}
```

**File: `test/test_vcard_store/test_vcard_store.cpp`**
```cpp
#include "mod_vcard/vcard_store.h"
#include "../../components/mod_vcard/src/vcard_store.cpp"  // Direct .cpp include!
#include <cstring>

void test_vcard_validate() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));
}
```

**File: `test/test_vcard_module_link/test_vcard_module_link.cpp`** (different pattern)
```cpp
#include "mod_vcard/VcardModule.h"

extern "C" void mod_vcard_register();  // Just declaration, no .cpp include

void test_vcard_module_link() {
    mod_vcard_register();
}
```

## Recommended Fix

**Option 1: Remove direct `.cpp` includes**

Let the build system handle linking:

```cpp
// test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp
#include "mod_vcard/ble_vcard.h"

void test_ble_vcard_symbols() {
    ble_vcard_init();
    ble_vcard_set_exchange_enabled(true);
}
```

Then ensure the test is compiled with the module's `.cpp` files via CMake.

**Option 2: Create test-specific build configuration**

If tests need special compilation, create a proper CMake target:

```cmake
# In test/CMakeLists.txt
idf_component_register(
    SRCS "test_ble_vcard_symbols.cpp"
    REQUIRES mod_vcard
    INCLUDE_DIRS "."
)
```

**Option 3: Use header-only test helpers**

If tests need access to internal functions, create a test header:

```cpp
// components/mod_vcard/include/mod_vcard/ble_vcard_test.h
#pragma once
// Declare internal functions for testing
void ble_vcard_init_internal();
```

Then include that in tests.

## References

- [Testing Implementation vs Interface](https://www.artima.com/weblogs/viewpost.jsp?thread=11088)
- [CMake Testing Best Practices](https://cliutils.gitlab.io/modern-cmake/chapters/testing.html)
