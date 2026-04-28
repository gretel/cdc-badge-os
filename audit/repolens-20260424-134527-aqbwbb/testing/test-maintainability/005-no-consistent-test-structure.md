---
title: "[LOW] Tests don't follow consistent structure (Arrange-Act-Assert)"
severity: LOW
domain: test-maintainability
lens: test-maintainability
labels:
  - "audit:testing/test-maintainability"
---

## Summary
The existing tests lack a clear structure following the Arrange-Act-Assert (AAA) pattern. While simple tests may not need explicit separation, larger tests should organize code into:
1. **Arrange**: Set up test data and preconditions
2. **Act**: Execute the code under test
3. **Assert**: Verify expected results

Current tests mix these phases without clear organization.

## Impact
- **Hard to read**: Developers must parse test logic to understand intent
- **Hard to maintain**: Adding new test scenarios requires understanding implicit structure
- **Inconsistent**: Future tests may follow different patterns

## Evidence
- `test/test_vcard_store/test_vcard_store.cpp:9-13` - All code in single function without AAA structure
- `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp:8-11` - No structure for setup/cleanup
- `test/test_vcard_module_link/test_vcard_module_link.cpp:9-11` - Simple case, structure acceptable

## Recommended Fix
Refactor tests to follow AAA pattern with clear comments:

```cpp
void test_vcard_validate() {
    // Arrange
    char err[64];
    const char* vcard_str = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    size_t vcard_len = strlen(vcard_str);
    
    // Act
    bool result = vcard_store_set_own(vcard_str, vcard_len, err, sizeof(err));
    
    // Assert
    assert(result == true);
    assert(strlen(err) == 0);
}
```

For more complex tests, consider separate setup/teardown functions:
```cpp
void setup_vcard_test() { /* ... */ }
void teardown_vcard_test() { /* ... */ }
void test_vcard_validate() {
    setup_vcard_test();
    // Act and Assert
    teardown_vcard_test();
}
```

## References
- [Arrange-Act-Assert Pattern](https://www.testingexcellence.com/arrange-act-assert-a-popular-format-for-unit-tests/)
- [AAA vs GWT Testing Patterns](https://stackoverflow.com/questions/1171236/what-is-the-difference-between-arrange-act-assert-and-given-when-then)
