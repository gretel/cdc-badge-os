---
title: "[MEDIUM] No test documentation or guide for developers"
severity: MEDIUM
domain: testing/test-quality
lens: documentation
labels:
  - "test-quality"
  - "documentation"
  - "developer-experience"
---

## Summary

The repository has **no documentation** about:
- How to write tests for the project
- Where to put new tests
- How to run tests
- What test structure/patterns to follow
- How tests integrate with CI/CD

The `test/` directory exists but there's no `README.md` or guide explaining its purpose.

## Impact

**Developer onboarding:**
- New contributors don't know how to add tests
- Unclear where to place tests for new modules
- No guidance on test patterns or conventions

**Test consistency:**
- Without guidance, each developer may write tests differently
- Tests may not follow a consistent structure
- Hard to maintain test quality over time

**Test coverage:**
- Developers may skip tests if they don't know how to write them
- Tests may be written in ad-hoc ways
- No expectation of test coverage for new features

**CI/CD integration:**
- Unclear how tests are run in CI
- No documentation on test reporting
- May lead to tests being skipped or not integrated properly

## Evidence

**Missing documentation files:**
- `test/README.md` - Does not exist
- `docs/TESTING.md` - Does not exist
- `CONTRIBUTING.md` - Does not exist (no test section)

**Existing test structure (no documentation):**
```
test/
├── test_vcard_module_link/
│   └── test_vcard_module_link.cpp
├── test_ble_vcard_symbols/
│   └── test_ble_vcard_symbols.cpp
└── test_vcard_store/
    └── test_vcard_store.cpp
```

**No test instructions in main README:**
- `README.md` mentions build and flash commands
- No mention of running tests
- No test coverage badges or status

**No test instructions in docs/README.md:**
- Lists module development, serial commands, UI flows
- No testing section

## Recommended Fix

**1. Create `test/README.md`:**

```markdown
# CDC Badge OS Tests

## Running Tests

Tests are built as separate ESP-IDF components. To run a test:

```bash
# Build a specific test
~/.platformio/penv/bin/pio run -t build --environment test_vcard_module_link

# Run tests via serial monitor
~/.platformio/penv/bin/pio device monitor
```

## Test Structure

Each test is a separate directory with its own CMakeLists.txt:

```
test/
├── test_<name>/
│   ├── CMakeLists.txt
│   └── test_<name>.cpp
```

## Writing Tests

See [TESTING_GUIDE.md](../docs/TESTING_GUIDE.md) for detailed guidance.

## Test Coverage

Current coverage:
- vCard module: basic link test
- BLE vCard: symbol test
- vCard store: validation test
```

**2. Create `docs/TESTING_GUIDE.md`:**

```markdown
# Testing Guide

## Test Patterns

### Basic Test Structure

```cpp
#include "<module>/<header>.h"

void test_<module>_<function>() {
    // Arrange
    // Act
    // Assert
}

extern "C" void app_main() {
    test_<module>_<function>();
}
```

### Assertions

Use standard C `assert()` for simple tests:

```cpp
int result = some_function();
assert(result == EXPECTED_VALUE);
```

For more complex tests, check return values:

```cpp
char err[64];
some_function(arg1, arg2, err, sizeof(err));
assert(err[0] == '\0');  // No error
```

### Error Case Testing

Always test error cases:

```cpp
void test_<module>_<function>_error() {
    // Test with invalid input
    // Verify error handling
}
```

## Adding a New Test

1. Create directory: `test/test_<name>/`
2. Add `CMakeLists.txt`
3. Write test in `test_<name>.cpp`
4. Add to build system
```

**3. Update `README.md`:**

Add a Testing section:

```markdown
## Testing

Run tests:
```bash
~/.platformio/penv/bin/pio run -t build --environment test_*
```

See [test/README.md](test/README.md) for details.
```

## References

- [ESP-IDF Testing Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/unit_test.html)
- [Test Documentation Best Practices](https://martinfowler.com/articles/practical-test-pyramid.html)
