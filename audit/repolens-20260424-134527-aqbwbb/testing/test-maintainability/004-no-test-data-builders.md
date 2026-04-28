---
title: "[MEDIUM] No test data builders or fixtures for complex objects"
severity: MEDIUM
domain: test-maintainability
lens: test-maintainability
labels:
  - "audit:testing/test-maintainability"
---

## Summary
Tests lack helper functions, fixtures, or builders for constructing test data. The vCard test has a hardcoded string that would benefit from a builder pattern:

**test_vcard_store.cpp:11**
```cpp
const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
```

This inline construction makes tests hard to read and maintain.

## Impact
- **Code duplication**: If multiple tests need similar vCard data, they must duplicate the string
- **Hard to read**: Long strings obscure test intent
- **Fragile**: Small changes to data format require updating multiple places
- **No reusability**: Cannot easily create variations of test data

## Evidence
- `test/test_vcard_store/test_vcard_store.cpp:11` - Inline vCard string with no helper
- `test/test_vcard_module_link/test_vcard_module_link.cpp` - No test data construction needed (simple case)
- `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp` - No test data construction needed (simple case)

## Recommended Fix
Create test helper functions or a vCard builder:

**components/mod_vcard/tests/vcard_test_helpers.h**
```cpp
#pragma once
#include <string>

inline std::string make_vcard(const char* fn, const char* version = "4.0") {
    return std::string("BEGIN:VCARD\nVERSION:") + version + 
           "\nFN:" + fn + "\nEND:VCARD\n";
}

inline std::string make_vcard_full(const char* fn, const char* tel, const char* email) {
    return std::string("BEGIN:VCARD\nVERSION:4.0\n") +
           "FN:" + fn + "\n" +
           "TEL:" + tel + "\n" +
           "EMAIL:" + email + "\n" +
           "END:VCARD\n";
}
```

**Updated test:**
```cpp
void test_vcard_validate() {
    char err[64];
    std::string v = make_vcard("Test");
    bool result = vcard_store_set_own(v.c_str(), v.length(), err, sizeof(err));
    assert(result == true);
}
```

## References
- [Test Data Builder Pattern](https://www.thoughtworks.com/insights/blog/test-data-builder-pattern)
- [Fixtures in Unit Testing](https://martinfowler.com/bliki/TestFixture.html)
