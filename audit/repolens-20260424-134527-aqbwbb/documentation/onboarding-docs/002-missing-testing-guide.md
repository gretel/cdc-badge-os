---
title: "[MEDIUM] Missing TESTING.md guide for running and writing tests"
severity: MEDIUM
domain: developer-onboarding
lens: onboarding-docs
labels:
  - "audit:documentation/onboarding-docs"
---

## Summary
The repository has a `test/` directory with unit tests but no documentation explaining:
- How to run tests locally
- What testing framework is used (ESP-IDF test framework)
- How to write new tests
- Test structure and conventions
- Where to place test files
- How to debug failing tests

**Evidence:**
- `test/` directory exists with 3 test modules:
  - `test_vcard_store/`
  - `test_vcard_module_link/`
  - `test_ble_vcard_symbols/`
- No `TESTING.md` or similar documentation
- README mentions build but not test commands

## Impact
New developers:
- Don't know how to verify their changes work
- Can't easily run the existing test suite
- Don't know how to add tests for new features
- May skip testing entirely due to uncertainty

This leads to:
- Less test coverage over time
- Bugs slipping through
- Inconsistent test patterns across modules

## Evidence
1. `test/` directory structure:
```
test/
├── test_vcard_store/
│   └── test_vcard_store.cpp
├── test_vcard_module_link/
│   └── test_vcard_module_link.cpp
└── test_ble_vcard_symbols/
    └── test_ble_vcard_symbols.cpp
```

2. Sample test file (`test/test_vcard_store/test_vcard_store.cpp`):
```cpp
#include "mod_vcard/vcard_store.h"
#include "../../components/mod_vcard/src/vcard_store.cpp"
#include <cstring>

void test_vcard_validate() {
    char err[64];
    const char* v = "BEGIN:VCARD\nVERSION:4.0\nFN:Test\nEND:VCARD\n";
    vcard_store_set_own(v, strlen(v), err, sizeof(err));
}

extern "C" void app_main() {
    test_vcard_validate();
}
```

3. No mention of testing in:
- `README.md` (no test command)
- `docs/README.md` (no testing section)
- `docs/MODULE_DEVELOPMENT.md` (no test examples)

4. Build workflow (`build.yml`) only runs `pio run` - no test step

## Recommended Fix
Create `TESTING.md` at repository root with:

1. **Overview**:
   - Testing framework used (ESP-IDF unit test framework)
   - Where tests are located (`test/` directory)

2. **Running Tests**:
   ```bash
   # Build test target (example - verify actual command)
   ~/.platformio/penv/bin/pio run -t test
   
   # Or run specific test
   ~/.platformio/penv/bin/pio test -e cdc_badge_usb -d test/test_vcard_store
   ```

3. **Test Structure**:
   - Each test in `test/<test_name>/`
   - `app_main()` as entry point
   - Include headers from component being tested

4. **Writing New Tests**:
   - Create directory `test/test_<feature>/`
   - Add `test_<feature>.cpp` with test functions
   - Register test in appropriate CMakeLists.txt
   - Example template

5. **Debugging Tests**:
   - Serial output at 115200 baud
   - Common failure patterns
   - How to isolate failing test

6. **CI Integration**:
   - Tests run on `build.yml` (if configured)
   - Required to pass before merge

**Estimated effort:** ~45-60 minutes to research and document.

## References
- [ESP-IDF Unit Testing Framework](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/testing/unit-test.html)
- [PlatformIO Unified Testing](https://docs.platformio.org/en/latest/integration/unified-testing.html)
