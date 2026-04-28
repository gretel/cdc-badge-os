---
title: "[MEDIUM] No test configuration or build infrastructure"
severity: MEDIUM
domain: test-maintainability
lens: test-maintainability
labels:
  - "audit:testing/test-maintainability"
---

## Summary
The `/input/20260423-132359-oj8ayc/cdc-badge-os/test/` directory lacks:
1. CMakeLists.txt files for each test
2. Test runner configuration for PlatformIO
3. Build targets for running tests

The 3 test directories (`test_vcard_module_link`, `test_vcard_store`, `test_ble_vcard_symbols`) have no build configuration, making it unclear how to compile and run them.

## Impact
- **Hard to discover**: Developers don't know how to run tests
- **No automation**: Tests can't be added to CI/CD pipeline
- **Inconsistent setup**: Each test might need different configuration
- **Fragile**: Tests may not compile with current build system

## Evidence
- `test/test_vcard_module_link/` - No CMakeLists.txt
- `test/test_vcard_store/` - No CMakeLists.txt
- `test/test_ble_vcard_symbols/` - No CMakeLists.txt
- `platformio.ini` - No test environment configuration
- `main/CMakeLists.txt` - No test targets defined

## Recommended Fix
1. **Add test CMakeLists.txt files** for each test directory:
```cmake
idf_component_register(
    SRCS "test_vcard_module_link.cpp"
    INCLUDE_DIRS "."
    REQUIRES mod_vcard cdc_core
)
```

2. **Configure PlatformIO test environment** in `platformio.ini`:
```ini
[testenv:cdc_badge_usb]
platform = espressif32@6.12.0
board = cdc-badge-usb
framework = espidf
test_build_src = true
```

3. **Add test script** in `tools/` for running tests:
```python
#!/usr/bin/env python3
# tools/run_tests.py
import subprocess
subprocess.run(["pio", "test", "-e", "cdc_badge_usb"])
```

## References
- [PlatformIO Test Framework](https://docs.platformio.org/en/latest/integrations/ide/vscode.html#testing)
- [ESP-IDF Build System](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html)
