---
title: "[HIGH] Tests lack assertions - provide false confidence"
severity: HIGH
domain: testing/test-quality
lens: test-assertions
labels:
  - "test-quality"
  - "missing-assertions"
---

## Summary

The main project test suite in `test/` directory contains 3 test files that execute functions but **have zero assertions** to verify correct behavior:

1. **`test/test_vcard_module_link/test_vcard_module_link.cpp`** (19 lines)
   - Calls `mod_vcard_register()` but never verifies the result
   - No check that registration succeeded or module is properly linked

2. **`test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp`** (19 lines)
   - Calls `ble_vcard_init()` and `ble_vcard_set_exchange_enabled(true)`
   - No verification that functions executed correctly or returned expected values

3. **`test/test_vcard_store/test_vcard_store.cpp`** (21 lines)
   - Calls `vcard_store_set_own()` with a sample vCard
   - No check that the vCard was stored, validated, or that `err` contains expected value

## Impact

**False confidence in code quality:**
- Tests that don't assert anything can pass even if the code under test fails completely
- A developer could delete the entire implementation and tests would still "pass"
- No regression protection - broken code won't be caught by the test suite
- CI/CD pipelines using these tests provide no actual validation

**Maintenance burden:**
- Developers may assume tests exist and are comprehensive when they're essentially empty
- New contributors may follow the pattern of writing "tests" without assertions

## Evidence

**File: `test/test_vcard_module_link/test_vcard_module_link.cpp`**
```cpp
void test_vcard_module_link() {
    mod_vcard_register();  // No return value checked, no assertion
}

extern "C" void app_main() {
    test_vcard_module_link();  // No result verification
}
```

**File: `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp`**
```cpp
void test_ble_vcard_symbols() {
    ble_vcard_init();
    ble_vcard_set_exchange_enabled(true);  // No return/state verification
}
```

**File: `test/test_vcard_store/test_vcard_store.cpp`**
```cpp
void test_vcard_validate() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));  // err never checked
}
```

**Verification:** Running `grep -r "ASSERT\|assert\|expect"` in the test directory returns no results.

## Recommended Fix

Add appropriate assertions to each test to verify actual behavior:

**For `test_vcard_module_link.cpp`:**
- Verify module registration by checking if the module appears in the registry
- Or verify a symbol/function from the module is accessible after registration

**For `test_ble_vcard_symbols.cpp`:**
- Check return values of `ble_vcard_init()` and `ble_vcard_set_exchange_enabled()`
- Or verify state changes (e.g., check that exchange enabled flag is set)

**For `test_vcard_store.cpp`:**
- Check the `err` buffer for success/failure codes
- Verify the vCard was actually stored by reading it back
- Test with invalid vCard data and verify error handling

**Example pattern to follow:**
```cpp
void test_vcard_store() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));
    
    // Add assertions
    assert(err[0] == '\0' || strcmp(err, "OK") == 0);  // No error
    // Or check return value if function returns status
}
```

## References

- [Testing Best Practices - Verify Observable Behavior](https://testing.googleblog.com/2010/12/test-tiers.html)
- [Common Testing Mistakes - Assertions](https://martinfowler.com/articles/practical-test-pyramid.html)
- Python test runner (`third_party/libtropic/scripts/test_runner/lt_test_runner/lt_test_runner.py` line 69) already has logic to warn when tests have no asserts - this should be applied to main project tests too
