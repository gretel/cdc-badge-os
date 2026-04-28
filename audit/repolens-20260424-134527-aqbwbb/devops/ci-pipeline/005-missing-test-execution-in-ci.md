---
title: "[MEDIUM] Tests exist but are not run in CI pipeline"
severity: MEDIUM
domain: devops
lens: ci-pipeline
labels:
  - "audit:devops/ci-pipeline"
---

## Summary
The repository contains test files in `test/` directory (3 test modules: `test_ble_vcard_symbols`, `test_vcard_module_link`, `test_vcard_store`), but the CI pipeline does not execute any tests. The build.yml only compiles the firmware but never runs the test suite.

**Evidence:**
- File: `.github/workflows/build.yml` - Lines 1-90
- Test files exist: `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp`, etc.
- CI pipeline only runs `pio build`, no test execution step

## Impact
- Test failures do not block merges to main
- Developers have no CI feedback on test status
- Tests may rot (become outdated) without regular execution
- Reduced confidence in code changes

## Evidence
```bash
# Test files exist:
test/
├── test_ble_vcard_symbols/
│   └── test_ble_vcard_symbols.cpp
├── test_vcard_module_link/
│   └── test_vcard_module_link.cpp
└── test_vcard_store/
    └── test_vcard_store.cpp
```

```yaml
# build.yml only builds, never runs tests:
- name: Build firmware
  run: pio run
# No test step
```

## Recommended Fix
Add a test execution step to `.github/workflows/build.yml`:

```yaml
- name: Run tests
  run: pio test -e native
# Or whatever test environment is configured
# If using ESP-IDF test framework:
- name: Run ESP-IDF tests
  run: idf.py test
```

Note: The test framework and command needs to be determined based on how tests are structured. The current tests use `app_main()` entry point, suggesting they may be ESP-IDF tests that need to be flashed to hardware or run in a simulator.

## References
- [PlatformIO Testing](https://docs.platformio.org/en/latest/integrations/ide/vscode/testing.html)
- [ESP-IDF Unit Testing](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/testing/index.html)
