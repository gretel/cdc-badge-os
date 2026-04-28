---
title: "[MEDIUM] Test suite has no CI/CD integration in GitHub Actions"
severity: MEDIUM
domain: test-suite
lens: toolgate/test-suite
labels:
  - "audit:toolgate/test-suite"
---

## Summary
The GitHub Actions workflow (`.github/workflows/build.yml`) only builds the main firmware but does not build or run the test suite. The test directory structure exists but is completely untested in CI.

Current build.yml only runs:
```yaml
- name: Build firmware
  run: pio run
```

No test building, execution, or reporting is configured.

## Impact
- **Unverified tests** - Tests may be broken without detection
- **No regression guard** - Code changes can break tests without CI notification
- **Manual testing required** - Developers must manually build and flash tests
- **CI/CD gap** - Test suite provides no value if not run automatically

## Evidence
From `.github/workflows/build.yml`:
```yaml
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout repository
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Setup Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.11'

      - name: Install PlatformIO
        run: |
          python -m pip install --upgrade pip
          pip install platformio

      - name: Build firmware
        run: pio run  # <-- Only builds main firmware, not tests

      - name: Rename firmware artifacts
        # ...
```

The test directory exists:
```
test/
├── test_ble_vcard_symbols/
├── test_vcard_module_link/
└── test_vcard_store/
```

But there's no workflow step to build or run these tests.

## Recommended Fix
Add test building to GitHub Actions workflow:

**Option 1: PlatformIO test command (if tests are in PlatformIO structure)**
```yaml
- name: Build and run tests
  run: pio test
```

**Option 2: ESP-IDF test command (for ESP-IDF test structure)**
```yaml
- name: Build tests
  run: |
    cd test/test_vcard_store
    idf.py build

- name: Run tests
  run: |
    idf.py test
```

**Option 3: Custom test workflow**
Create `.github/workflows/test.yml`:
```yaml
name: Test Suite

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Setup ESP-IDF
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.2

      - name: Build and run tests
        run: |
          for test_dir in test/test_*/; do
            cd $test_dir
            idf.py build
            idf.py test
            cd ../..
          done
```

## References
- [GitHub Actions for ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/testing/building-tests.html#building-tests)
- [PlatformIO testing](https://docs.platformio.org/en/latest/advanced/testing/frameworks/index.html)
- [ESP-IDF CI examples](https://github.com/espressif/esp-idf/tree/master/.github/workflows)
