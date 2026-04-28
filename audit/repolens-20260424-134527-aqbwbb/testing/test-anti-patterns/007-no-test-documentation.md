---
title: "[LOW] No test documentation or README"
severity: LOW
domain: testing
lens: test-anti-patterns
labels:
  - "test-documentation"
---

## Summary
No documentation exists explaining how to run tests, what tests cover, or the testing strategy. New developers must guess how to use the test files.

## Impact
- **High onboarding cost**: New developers don't know how to run tests
- **Tests may be forgotten**: If tests aren't documented, they may be ignored
- **No testing strategy**: No guidance on what to test or how to write new tests

## Evidence
- No `test/README.md` or `TESTING.md` file
- No test documentation in main README
- No comments explaining test structure
- Test files have minimal documentation (just function-level comments)

## Recommended Fix
1. Create `test/README.md` with:
   - How to run tests
   - Test structure and organization
   - How to add new tests
   - CI integration details

2. Add testing section to main README:
   ```markdown
   ## Testing
   
   Run tests with:
   ```bash
   pio test -e cdc_badge_usb
   ```
   ```

3. Document test naming conventions:
   - `test_<module>_<feature>.cpp`
   - Each test file should have a descriptive name

## References
- Testing Documentation Best Practices: https://testing.googleblog.com/2010/12/tested-not-tested.html
- ESP-IDF Testing Guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/unit-tests.html
