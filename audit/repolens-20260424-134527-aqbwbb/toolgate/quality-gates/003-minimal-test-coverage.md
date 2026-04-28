---
title: "[HIGH] Minimal test coverage for firmware codebase"
severity: HIGH
domain: testing
lens: quality-gates
labels:
  - "audit:toolgate/quality-gates"
  - "testing"
---

## Summary
The codebase has only 3 test files for 764 source files, providing minimal test coverage for a firmware project used as a hardware security key.

**Evidence:**
- Test files found:
  - `test/test_vcard_module_link/test_vcard_module_link.cpp` (linking smoke test)
  - `test/test_vcard_store/test_vcard_store.cpp`
  - `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp`
- Only 3 test files vs ~764 source files
- No CI test step in `.github/workflows/build.yml`
- No test framework configuration (e.g., CTest, Unity, CppUTest)

## Impact
- Critical security features (FIDO2, GPG, TOTP, Password Vault) lack automated tests
- Regression bugs more likely when modifying core functionality
- Module isolation not verified programmatically
- TROPIC01 secure element integration not tested
- For a hardware security key, lack of tests increases risk of authentication bugs

## Recommended Fix
1. Add a test framework (Unity or CppUTest recommended for ESP-IDF)
2. Create CI step to run tests on every build:
   ```yaml
   - name: Run tests
     run: pio test
   ```
3. Prioritize tests for:
   - TROPIC01 secure element operations
   - PIN management and lockout logic
   - FIDO2/WebAuthn authentication flow
   - Module registration and lifecycle

## References
- [ESP-IDF Unit Testing](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/unit-test.html)
- [PlatformIO Unit Testing](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html)
