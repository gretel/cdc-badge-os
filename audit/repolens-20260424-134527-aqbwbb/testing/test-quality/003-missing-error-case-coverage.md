---
title: "[MEDIUM] Tests only cover happy path - no error case validation"
severity: MEDIUM
domain: testing/test-quality
lens: error-cases
labels:
  - "test-quality"
  - "error-cases"
  - "edge-cases"
---

## Summary

All three tests in the `test/` directory only test the **happy path** with valid inputs. None of them verify that the code handles:

1. **Invalid inputs** (e.g., malformed vCard, null pointers)
2. **Error conditions** (e.g., registration failure, initialization failure)
3. **Edge cases** (e.g., empty strings, maximum length inputs)
4. **Failure modes** (e.g., what happens when hardware is unavailable)

## Impact

**Undetected regressions:**
- Error handling code paths are never executed in tests
- Bug fixes to error handling may break silently
- New error cases introduced during refactoring won't be caught

**Incomplete validation:**
- Tests give false confidence that the module is "working"
- Real-world usage (with imperfect data) may reveal untested failure modes
- Edge cases that cause crashes or undefined behavior are missed

**Security implications:**
- Input validation is critical for security modules (vCard, BLE)
- Missing error case tests may mean missing security validation
- Attack surface (malformed input) is not verified

## Evidence

**File: `test/test_vcard_store/test_vcard_store.cpp`**

Only tests valid vCard:
```cpp
const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
vcard_store_set_own(v, strlen(v), err, sizeof(err));
```

**Missing error cases:**
```cpp
// Should also test:
// - Malformed vCard (missing END:VCARD)
// - Empty vCard string
// - NULL pointer
// - Very long vCard (buffer overflow check)
// - Invalid characters
// - Duplicate entries
```

**File: `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp`**

Only tests successful init:
```cpp
ble_vcard_init();
ble_vcard_set_exchange_enabled(true);
```

**Missing error cases:**
```cpp
// Should also test:
// - What if init() is called twice?
// - What if init() fails (return value not checked)?
// - What if set_exchange_enabled() is called before init()?
// - What if BLE is unavailable?
```

**File: `test/test_vcard_module_link/test_vcard_module_link.cpp`**

No error checking at all:
```cpp
mod_vcard_register();
```

**Missing error cases:**
```cpp
// Should also test:
// - What if register() is called twice?
// - What if module already exists?
// - What if registration fails?
```

## Recommended Fix

**1. Add error case tests for vcard_store:**

```cpp
void test_vcard_store_invalid_vcard() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test";  // Missing END:VCARD
    vcard_store_set_own(v, strlen(v), err, sizeof(err));
    assert(err[0] != '\0');  // Should have error message
}

void test_vcard_store_empty_string() {
    char err[64];
    const char* v = "";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));
    assert(err[0] != '\0');  // Should have error message
}

void test_vcard_store_null_pointer() {
    char err[64];
    vcard_store_set_own(NULL, 0, err, sizeof(err));
    assert(err[0] != '\0');  // Should handle gracefully
}
```

**2. Add error case tests for ble_vcard:**

```cpp
void test_ble_vcard_double_init() {
    ble_vcard_init();
    int result = ble_vcard_init();  // Should return error or handle gracefully
    assert(result == 0 || result == 1);  // Verify expected behavior
}

void test_ble_vcard_enable_before_init() {
    ble_vcard_set_exchange_enabled(true);  // Should handle gracefully
}
```

**3. Add error case tests for vcard_module:**

```cpp
void test_vcard_module_double_register() {
    mod_vcard_register();
    mod_vcard_register();  // Should handle gracefully or return status
}
```

## References

- [Testing Error Handling](https://www.agiledata.org/essays/testingErrorHandling.html)
- [Edge Case Testing Guide](https://www.guru99.com/edge-case-testing.html)
- [Input Validation Testing](https://www.softwaretestinghelp.com/input-data-testing/)
