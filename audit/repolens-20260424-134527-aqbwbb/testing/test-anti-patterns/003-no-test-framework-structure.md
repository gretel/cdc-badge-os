---
title: "[MEDIUM] No test framework - each test is a standalone app_main"
severity: MEDIUM
domain: testing
lens: test-anti-patterns
labels:
  - "test-structure"
---

## Summary
Each test file defines its own `app_main()` function, making it impossible to run multiple tests together. There is no test framework, test runner, or suite structure - each test is essentially a standalone firmware image.

Files affected:
- `test/test_vcard_module_link/test_vcard_entry.cpp:17` - `extern "C" void app_main()`
- `test/test_vcard_store/test_vcard_store.cpp:19` - `extern "C" void app_main()`
- `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp:17` - `extern "C" void app_main()`

## Impact
- **Cannot run tests together**: Each test must be flashed separately to run
- **No test discovery**: No way to list or discover available tests
- **No test grouping**: Cannot run tests by category or module
- **No setup/teardown**: No mechanism for shared test fixtures or cleanup
- **No test reporting**: No structured output about pass/fail status
- **CI/CD difficulty**: Hard to integrate into automated testing pipelines

## Evidence
```cpp
// Each test file has this pattern:
extern "C" void app_main() {
    test_vcard_module_link();
}
```

Compare to proper ESP-IDF test structure:
```cpp
// Should be like ESP-IDF unit tests:
TEST_CASE("vcard module registers correctly", "mod_vcard") {
    mod_vcard_register();
    TEST_ASSERT_TRUE(module_was_registered());
}

TEST_CASE("vcard store validates input", "mod_vcard") {
    // ...
}
```

## Recommended Fix
1. Adopt ESP-IDF's built-in unit test framework using `TEST_CASE()` macros
2. Create a test runner that can execute multiple test cases
3. Structure tests with proper setup/teardown:
   ```cpp
   TEST_CASE("vcard store validates input", "mod_vcard") {
       // Arrange
       char err[64] = {0};
       const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
       
       // Act
       vcard_store_set_own(v, strlen(v), err, sizeof(err));
       
       // Assert
       TEST_ASSERT_EQUAL_STRING("", err);
   }
   ```
4. Add a test CMakeLists.txt that properly configures the test runner

## References
- ESP-IDF Unit Testing: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/unit-tests.html
- Unity Test Framework: https://github.com/ThrowTheSwitch/Unity
