---
title: "[LOW] Test suite missing ESP-IDF Unity test framework integration"
severity: LOW
domain: test-suite
lens: toolgate/test-suite
labels:
  - "audit:toolgate/test-suite"
---

## Summary
The test suite uses custom test functions with `app_main()` entry points instead of the ESP-IDF Unity test framework. This means:
- No test discovery/execution automation
- No formatted test output (pass/fail counts)
- No test fixtures or setup/teardown support
- Manual firmware flashing required for each test run

## Impact
- **Inefficient testing workflow** - Each test runs as separate firmware with full boot
- **No test aggregation** - Cannot run multiple tests in one firmware image
- **Limited CI/CD integration** - Hard to parse results in GitHub Actions
- **Missed ESP-IDF features** - No access to test filtering, fixtures, memory leak detection

## Evidence
Current test structure uses standalone `app_main()`:

```cpp
// test/test_vcard_store/test_vcard_store.cpp
void test_vcard_validate() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));
}

extern "C" void app_main() {
    test_vcard_validate();
}
```

Compare to ESP-IDF Unity test structure:
```cpp
#include "unity.h"
#include "mod_vcard/vcard_store.h"

void test_vcard_validate(void) {
    // ...
}

void app_main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_vcard_validate);
    UNITY_END();
}
```

ESP-IDF's test framework provides:
- `TEST_ASSERT_TRUE()` / `TEST_ASSERT_FALSE()` macros
- `RUN_TEST()` macro for test execution
- Automatic pass/fail reporting
- Test filtering via `idf.py test`

## Recommended Fix
Migrate tests to ESP-IDF Unity framework:

**Step 1:** Add Unity to dependencies in CMakeLists.txt:
```cmake
idf_component_register(
    SRCS "test_vcard_store.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        mod_vcard
        unity  # <-- Add Unity
)
```

**Step 2:** Update test file structure:
```cpp
#include "unity.h"
#include "mod_vcard/vcard_store.h"
#include <cstring>

void test_vcard_validate(void) {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    
    TEST_ASSERT_TRUE(vcard_store_set_own(v, strlen(v), err, sizeof(err)));
    
    char out[256];
    size_t len = vcard_store_get_own(out, sizeof(out));
    TEST_ASSERT_NOT_EQUAL(0, len);
}

void app_main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_vcard_validate);
    UNITY_END();
}
```

**Step 3:** Run tests via ESP-IDF:
```bash
idf.py test
```

## References
- [ESP-IDF Unity Test Framework](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/testing/unity.html)
- [Running tests with idf.py](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/testing/running-tests.html)
