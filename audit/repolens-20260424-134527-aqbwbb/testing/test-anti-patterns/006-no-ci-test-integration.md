---
title: "[MEDIUM] No CI test integration - tests not run in GitHub Actions"
severity: MEDIUM
domain: testing
lens: test-anti-patterns
labels:
  - "test-automation"
---

## Summary
The GitHub Actions workflow (`build.yml`) only builds the firmware but never runs the test suite. Tests exist in the repository but are not integrated into CI.

## Impact
- **Tests can go stale**: No guarantee tests still compile or pass after changes
- **Manual testing required**: Developers must remember to run tests manually
- **Delayed feedback**: Test failures only discovered when developers run tests
- **No quality gate**: Broken tests don't block merges or releases

## Evidence
Workflow file (`build.yml`):
```yaml
# Only builds, never runs tests
- name: Build firmware
  run: pio run

# No test step at all
```

Test files exist but are never executed:
- `test/test_vcard_module_link/test_vcard_module_link.cpp`
- `test/test_vcard_store/test_vcard_store.cpp`
- `test/test_ble_vcard_symbols/test_ble_vcard_symbols.cpp`

## Recommended Fix
1. Add a test step to the GitHub Actions workflow:
   ```yaml
   - name: Run tests
     run: |
       # If tests need to be built separately
       pio test -e cdc_badge_usb
       
       # Or run test discovery
       find test -name "*.cpp" -exec echo "Found: {}" \;
   ```

2. Create a test-specific build target in PlatformIO:
   ```ini
   [env:test]
   extends: cdc_badge_usb
   build_src_filter = +<test/>
   ```

3. Add a Makefile or script target for running tests:
   ```bash
   # Makefile
   test:
       pio test -e cdc_badge_usb
   ```

4. Update workflow to fail on test failures:
   ```yaml
   - name: Build and test
     run: |
       pio run
       pio test -e cdc_badge_usb --verbose
   ```

## References
- GitHub Actions: https://docs.github.com/en/actions
- PlatformIO Testing: https://docs.platformio.org/en/latest/advanced/testing/overview.html
- ESP-IDF Unit Tests in CI: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/unit-tests.html
