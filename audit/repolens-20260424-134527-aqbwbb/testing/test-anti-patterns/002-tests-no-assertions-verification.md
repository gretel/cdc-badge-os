---
title: "[HIGH] Tests lack assertions - no verification of results"
severity: HIGH
domain: testing
lens: test-anti-patterns
labels:
  - "test-structure"
---

## Summary
All three test functions execute code but never verify results with assertions:

1. `test_vcard_module_link()` - calls `mod_vcard_register()` but doesn't verify anything
2. `test_vcard_validate()` - calls `vcard_store_set_own()` but doesn't check the result or error buffer
3. `test_ble_vcard_symbols()` - calls `ble_vcard_init()` and `ble_vcard_set_exchange_enabled()` but doesn't verify state

## Impact
- **False positives**: Tests pass even if the functions return errors or produce incorrect results
- **No confidence**: Running tests gives a false sense of security - they only verify the code doesn't crash
- **Undetected regressions**: Changes to function behavior won't be caught by tests
- **Minimal test value**: These are essentially smoke tests that could be replaced with a simple build check

## Evidence
```cpp
// test/test_vcard_module_link/test_vcard_module_link.cpp:9-11
void test_vcard_module_link() {
    mod_vcard_register();
    // No assertion, no return value check
}

// test/test_vcard_store/test_vcard_store.cpp:9-13
void test_vcard_validate() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));
    // No check of err buffer, no verification of state
}

// test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp:8-11
void test_ble_vcard_symbols() {
    ble_vcard_init();
    ble_vcard_set_exchange_enabled(true);
    // No verification of state or return values
}
```

## Recommended Fix
Add assertions to verify expected behavior:

1. **test_vcard_module_link**: Verify the registration actually happened (check if a global registry contains the module)
2. **test_vcard_validate**: Check that `err` is empty or contains expected value, verify vcard was stored correctly
3. **test_ble_vcard_symbols**: Verify `ble_vcard_init()` returns success, check that exchange enabled state is actually set

Example fix pattern:
```cpp
void test_vcard_validate() {
    char err[64] = {0};
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));
    // Add assertion
    assert(err[0] == '\0' || strcmp(err, "OK") == 0);
}
```

## References
- Testing Pyramid: https://martinfowler.com/articles/practical-test-pyramid.html
- ESP-IDF Test Framework: Uses `TEST_CASE()` with `TEST_ASSERT_*` macros for verification
