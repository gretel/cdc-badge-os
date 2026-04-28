---
title: "[HIGH] Test suite lacks CMakeLists.txt - tests cannot be built"
severity: HIGH
domain: test-suite
lens: toolgate/test-suite
labels:
  - "audit:toolgate/test-suite"
---

## Summary
The test suite in `/input/20260423-132359-oj8ayc/cdc-badge-os/test/` contains three test directories but none have `CMakeLists.txt` files required to build them with ESP-IDF:

- `test/test_ble_vcard_symbols/` - missing CMakeLists.txt
- `test/test_vcard_module_link/` - missing CMakeLists.txt
- `test/test_vcard_store/` - missing CMakeLists.txt

Without CMakeLists.txt files, the tests cannot be compiled or linked into a runnable firmware image.

## Impact
- **Test suite is non-functional** - Tests exist but cannot be built or executed
- **No automated testing** - Developers must manually verify changes
- **Regression risk** - Code changes may break functionality without detection
- **CI/CD gap** - GitHub Actions (build.yml) only builds main firmware, not tests

## Evidence
Directory structure shows test files exist but no build configuration:

```
test/
├── test_ble_vcard_symbols/
│   └── test_ble_vcard_symbols.cpp  (378 bytes)
├── test_vcard_module_link/
│   └── test_vcard_module_link.cpp  (337 bytes)
└── test_vcard_store/
    └── test_vcard_store.cpp        (339 bytes)
```

Running `find test/ -name "CMakeLists.txt"` returns no results.

ESP-IDF requires each component/test directory to have a `CMakeLists.txt` with `idf_component_register()` to define sources, include paths, and dependencies.

## Recommended Fix
Create `CMakeLists.txt` for each test directory. Example for `test/test_vcard_store/`:

```cmake
idf_component_register(
    SRCS
        "test_vcard_store.cpp"
    INCLUDE_DIRS
        "."
        "../.."
        "../../components/mod_vcard/include"
    REQUIRES
        mod_vcard
        cdc_log
        nvs_flash
        freertos
)

# Set test entry point
set_property(TARGET ${COMPONENT_LIB} PROPERTY ESP_IDF_MAIN_FUNC app_main)
```

Repeat for each test:
1. `test/test_vcard_module_link/CMakeLists.txt` - depends on `mod_vcard`
2. `test/test_ble_vcard_symbols/CMakeLists.txt` - depends on `mod_vcard`

## References
- [ESP-IDF CMake Reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/cmake.html)
- [ESP-IDF Unit Testing](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/testing/index.html)
