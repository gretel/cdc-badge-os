---
title: "[HIGH] Tests lack assertions - smoke tests don't verify behavior"
severity: HIGH
domain: test-maintainability
lens: test-maintainability
labels:
  - "audit:testing/test-maintainability"
---

## Summary
The test suite in `/input/20260423-132359-oj8ayc/cdc-badge-os/test/` contains 3 smoke tests that call functions but never assert expected results:

1. **test_vcard_module_link.cpp:9-11** - Calls `mod_vcard_register()` but doesn't verify registration succeeded
2. **test_vcard_store.cpp:9-13** - Calls `vcard_store_set_own()` but doesn't check return value or error buffer
3. **test_ble_vcard_symbols.cpp:8-11** - Calls `ble_vcard_init()` and `ble_vcard_set_exchange_enabled()` but doesn't verify state

Example from `test_vcard_store.cpp`:
```cpp
void test_vcard_validate() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));  // No assertion!
}
```

## Impact
- **False confidence**: Tests pass even when functionality is broken
- **Brittle**: Tests won't catch regressions since they don't verify behavior
- **Maintenance burden**: Developers must manually verify test coverage

## Evidence
- `test/test_vcard_module_link/test_vcard_module_link.cpp:9-11` - No assertion after `mod_vcard_register()`
- `test/test_vcard_store/test_vcard_store.cpp:9-13` - No assertion after `vcard_store_set_own()`
- `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp:8-11` - No assertion after function calls

## Recommended Fix
Add assertions to verify test outcomes:

1. **For vcard_module_link**: Add a check that the registration function is callable
2. **For vcard_store**: Check return value and error buffer contents
3. **For ble_vcard_symbols**: Verify internal state after initialization

Example fix for `test_vcard_store.cpp`:
```cpp
void test_vcard_validate() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    bool result = vcard_store_set_own(v, strlen(v), err, sizeof(err));
    assert(result == true);  // Verify success
    assert(strlen(err) == 0);  // Verify no error message
}
```

## References
- [Testing Best Practices - Arrange-Act-Assert pattern](https://www.testingexcellence.com/arrange-act-assert-a-popular-format-for-unit-tests/)
- [Common Testing Mistakes: Tests without assertions](https://stackoverflow.com/questions/27575463/unit-tests-without-assertions)
