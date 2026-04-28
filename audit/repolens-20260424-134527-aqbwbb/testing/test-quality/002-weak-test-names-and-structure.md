---
title: "[MEDIUM] Test names are generic and lack descriptive behavior"
severity: MEDIUM
domain: testing/test-quality
lens: test-names
labels:
  - "test-quality"
  - "test-names"
  - "documentation"
---

## Summary

The test functions in `test/` directory use generic, non-descriptive names that don't convey what behavior is being tested:

1. **`test_vcard_module_link()`** - What aspect of the link is tested? What should happen?
2. **`test_ble_vcard_symbols()`** - Which symbols? What's the expected outcome?
3. **`test_vcard_validate()`** - What validation? What's the expected result?

Additionally, tests lack:
- **`describe` blocks** or grouping structure
- **Test comments** explaining expected behavior
- **Clear test organization** (setup, action, verification phases)

## Impact

**Reduced discoverability:**
- Developers can't quickly understand what's being tested without reading the code
- Hard to identify which tests cover specific features or edge cases
- Onboarding new developers takes longer

**Maintenance difficulty:**
- When a test fails, it's not immediately obvious what behavior broke
- Hard to identify duplicate or overlapping tests
- Refactoring becomes riskier without clear test documentation

**Poor documentation:**
- Tests serve as living documentation; generic names lose this value
- Makes it harder to generate test reports or coverage summaries

## Evidence

**File: `test/test_vcard_module_link/test_vcard_module_link.cpp`**
```cpp
/**
 * \brief Link/symbol smoke test for vCard module registration.
 * \return void
 */
void test_vcard_module_link() {  // What exactly is being tested?
    mod_vcard_register();
}
```

**File: `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp`**
```cpp
/**
 * \brief Link/symbol smoke test for BLE vCard API.
 * \return void
 */
void test_ble_vcard_symbols() {  // Which symbols? What's verified?
    ble_vcard_init();
    ble_vcard_set_exchange_enabled(true);
}
```

**File: `test/test_vcard_store/test_vcard_store.cpp`**
```cpp
/**
 * \brief Smoke-test for vCard validation/store path.
 * \return void
 */
void test_vcard_validate() {  // What validation? Success or failure case?
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));
}
```

## Recommended Fix

**1. Rename tests to describe behavior:**

```cpp
// Before
void test_vcard_module_link() {

// After
void test_vcard_module_registers_successfully() {
```

```cpp
// Before
void test_ble_vcard_symbols() {

// After
void test_ble_vcard_init_and_enable_exchange() {
```

```cpp
// Before
void test_vcard_validate() {

// After
void test_vcard_store_accepts_valid_vcard() {
```

**2. Add structured test comments:**

```cpp
/**
 * \brief Tests that vCard module registration completes without error.
 *
 * Expected behavior:
 * - mod_vcard_register() is called
 * - Module appears in registry (verify via ModuleRegistry lookup)
 * - No crash or exception occurs
 *
 * \return void
 */
void test_vcard_module_registers_successfully() {
    // Arrange
    // Act
    mod_vcard_register();
    // Assert
    // Check module is in registry
}
```

**3. Consider adopting a test naming convention:**
- `test_<module>_<action>_<expected_result>`
- `test_<function>_<input>_<output>`

## References

- [Test Naming Best Practices](https://www.freecodecamp.org/news/how-to-write-better-test-names/)
- [Behavior-Driven Test Names](https://martinfowler.com/bliki/GherkinStyleTests.html)
