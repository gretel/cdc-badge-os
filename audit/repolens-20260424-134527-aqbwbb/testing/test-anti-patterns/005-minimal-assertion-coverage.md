---
title: "[LOW] Minimal assertion coverage in smoke tests"
severity: LOW
domain: testing
lens: test-anti-patterns
labels:
  - "audit:testing/test-anti-patterns"
---

## Summary
The smoke tests in `test/` directory call functions but don't verify their results with assertions. These tests only check that code compiles and links, not that the functions behave correctly.

**Affected files:**
- `test/test_vcard_module_link/test_vcard_module_link.cpp`
- `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp`
- `test/test_vcard_store/test_vcard_store.cpp`

## Impact
1. **False confidence**: Tests pass even if functions return errors or have no effect.
2. **Regression detection fails**: Changes that break functionality won't be caught.
3. **Tests don't validate behavior**: Only validates that symbols exist and can be called.

## Evidence
**`test/test_vcard_module_link/test_ble_vcard_symbols.cpp:9-11`:**
```cpp
void test_vcard_module_link() {
    mod_vcard_register();  // Called but result not checked
}
```

**`test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp:8-11`:**
```cpp
void test_ble_vcard_symbols() {
    ble_vcard_init();  // Called but result not checked
    ble_vcard_set_exchange_enabled(true);  // Called but result not checked
}
```

**`test/test_vcard_store/test_vcard_store.cpp:9-13`:**
```cpp
void test_vcard_validate() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));  // Called but result not checked
}
```

None of these tests use `LT_TEST_ASSERT` or any other assertion mechanism.

## Recommended Fix
Add assertions to verify expected behavior:

```cpp
void test_vcard_module_link() {
    // Should return void, but we can at least verify it doesn't crash
    // Consider adding a pre/post state check if applicable
    mod_vcard_register();
    // TODO: Add state verification after registration
}

void test_ble_vcard_symbols() {
    // Verify init succeeds
    LT_TEST_ASSERT(LT_OK, ble_vcard_init());
    
    // Verify set_exchange_enabled works (may need a getter to verify)
    ble_vcard_set_exchange_enabled(true);
    // TODO: Add getter like ble_vcard_get_exchange_enabled() to verify
}

void test_vcard_validate() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    // Verify the function returns success
    LT_TEST_ASSERT(LT_OK, vcard_store_set_own(v, strlen(v), err, sizeof(err)));
    // Verify error buffer is null (no error)
    LT_TEST_ASSERT(0, strlen(err));
}
```

## References
- [Assertion importance in tests](https://www.montsoue.com/2023/01/15/Testing-101-Assertions.html)
- [Smoke test best practices](https://stackoverflow.com/questions/2520895/what-is-a-smoke-test)
