---
title: "[MEDIUM] Test functions lack assertions - no verification of results"
severity: MEDIUM
domain: test-suite
lens: toolgate/test-suite
labels:
  - "audit:toolgate/test-suite"
---

## Summary
All three test functions call functions but never verify the results. They are essentially "smoke tests" that only check if code compiles and runs without crashing:

1. `test/test_vcard_module_link/test_vcard_module_link.cpp` - calls `mod_vcard_register()` but verifies nothing
2. `test/test_vcard_store/test_vcard_store.cpp` - calls `vcard_store_set_own()` but doesn't check return value or validate stored data
3. `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp` - calls `ble_vcard_init()` and `ble_vcard_set_exchange_enabled()` but checks nothing

## Impact
- **False positives** - Tests pass even if functions return errors or store wrong data
- **No regression detection** - Bugs can be introduced without tests catching them
- **Minimal coverage** - Only tests compilation/linking, not actual functionality
- **Developer time wasted** - Running tests gives no confidence in correctness

## Evidence
From `test/test_vcard_store/test_vcard_store.cpp`:
```cpp
void test_vcard_validate() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));  // Return value ignored!
}
```

The function `vcard_store_set_own()` returns `bool` and takes an error buffer, but:
- Return value is not checked (succeeded? failed?)
- Error buffer is not inspected
- No verification that data was stored correctly

From `test/test_vcard_module_link/test_vcard_module_link.cpp`:
```cpp
void test_vcard_module_link() {
    mod_vcard_register();  // No verification
}
```

From `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp`:
```cpp
void test_ble_vcard_symbols() {
    ble_vcard_init();  // Return value ignored
    ble_vcard_set_exchange_enabled(true);  // No verification
}
```

## Recommended Fix
Add assertions using ESP-IDF's Unity test framework or simple checks:

**Option 1: Simple assertions (quick fix)**
```cpp
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

void test_vcard_validate() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    
    bool result = vcard_store_set_own(v, strlen(v), err, sizeof(err));
    
    // Assert return value
    if (!result) {
        printf("FAIL: vcard_store_set_own returned error: %s\n", err);
        return;
    }
    
    // Assert data was stored
    char out[256];
    size_t len = vcard_store_get_own(out, sizeof(out));
    if (len == 0 || strcmp(out, v) != 0) {
        printf("FAIL: Stored vCard doesn't match\n");
        return;
    }
    
    printf("PASS: vcard_store_set_own\n");
}
```

**Option 2: Unity framework (recommended for full test suite)**
```cpp
#include "unity.h"
#include "mod_vcard/vcard_store.h"

void test_vcard_validate(void) {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    
    TEST_ASSERT_TRUE(vcard_store_set_own(v, strlen(v), err, sizeof(err)));
    
    char out[256];
    size_t len = vcard_store_get_own(out, sizeof(out));
    TEST_ASSERT_NOT_EQUAL(0, len);
    TEST_ASSERT_EQUAL_STRING(v, out);
}
```

## References
- [ESP-IDF Unity Testing](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/testing/unity.html)
- [Test-Driven Development basics](https://en.wikipedia.org/wiki/Test-driven_development)
