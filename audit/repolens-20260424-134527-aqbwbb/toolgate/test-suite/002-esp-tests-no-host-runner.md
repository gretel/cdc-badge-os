---
title: "[MEDIUM] ESP-IDF C++ tests lack host-side test runner"
severity: MEDIUM
domain: test-suite
lens: test-suite
labels:
  - "audit:toolgate/test-suite"
  - "setup"
  - "cpp"
---

## Summary
The C++ smoke tests in `test/` directory are designed to run on ESP32-S3 hardware but have no host-side test runner to execute them. These tests cannot be run without physical hardware or an ESP-IDF test framework setup.

**Test files:**
- `test/test_vcard_module_link/test_vcard_module_link.cpp` - Link/symbol test for vCard module registration
- `test/test_vcard_store/test_vcard_store.cpp` - Smoke test for vCard validation/store
- `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp` - Link/symbol test for BLE vCard API

Each test file has an `app_main()` entry point designed for ESP-IDF firmware build, not host execution.

## Impact
- **No unit test execution**: Core module functionality (vCard storage, BLE exchange) cannot be validated without flashing to hardware
- **CI/CD gap**: Continuous integration cannot run these tests without ESP32 hardware or simulator
- **Development friction**: Developers must flash firmware to test basic module functionality

## Evidence
Test file structure (from `test/test_vcard_module_link/test_vcard_module_link.cpp`):
```cpp
#include "mod_vcard/VcardModule.h"

extern "C" void mod_vcard_register();

void test_vcard_module_link() {
    mod_vcard_register();
}

extern "C" void app_main() {
    test_vcard_module_link();
}
```

The `app_main()` function is the ESP-IDF entry point, not a standard C++ test function. There is no:
- CMake test configuration in `test/` directory
- ESP-IDF `idf.py test` setup
- Host-side runner script to compile and run tests

## Recommended Fix
**Option 1: Add ESP-IDF test framework support**
1. Create `test/CMakeLists.txt` with `idf_component_register()` and `idf_add_test()`
2. Use ESP-IDF's built-in test framework (see [ESP-IDF Unit Testing docs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/testing/index.html))
3. Run with `idf.py test`

**Option 2: Add host-side test runner**
1. Create a Makefile or CMakeLists.txt in `test/` that compiles tests for host
2. Mock ESP-IDF dependencies (NVS, FreeRTOS, etc.)
3. Run tests on host machine before flashing

**Option 3: Integration test wrapper**
1. Create a Python script that:
   - Builds firmware with test configuration
   - Flashes to hardware (or simulator)
   - Monitors serial output for test results
2. Integrate with CI/CD pipeline

## References
- [ESP-IDF Testing Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/testing/index.html)
- [test_vcard_module_link.cpp](test/test_vcard_module_link/test_vcard_module_link.cpp)
- [test_vcard_store.cpp](test/test_vcard_store/test_vcard_store.cpp)
- [test_ble_vcard_symbols.cpp](test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp)
