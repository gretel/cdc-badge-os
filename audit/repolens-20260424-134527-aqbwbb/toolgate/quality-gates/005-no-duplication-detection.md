---
title: "[LOW] No code duplication detection in CI"
severity: LOW
domain: code-quality
lens: quality-gates
labels:
  - "audit:toolgate/quality-gates"
  - "maintainability"
---

## Summary
The project has no code duplication detection (e.g., `cpplint`, `clone-detection`) to identify repeated code patterns across 764 source files.

**Evidence:**
- No duplication check in `.github/workflows/build.yml`
- No `.cppcheck` or similar configuration
- Project emphasizes DRY principle (CLAUDE.md: "Extract common code into helpers") but has no enforcement

## Impact
- Code duplication may accumulate over time
- Maintenance burden increases with repeated patterns
- Bug fixes may need to be applied in multiple places
- Harder to ensure consistent implementation across modules

## Recommended Fix
1. Add `cppcheck` to CI for basic code quality checks:
   ```yaml
   - name: Install cppcheck
     run: sudo apt-get install -y cppcheck

   - name: Run cppcheck
     run: |
       cppcheck --enable=style,warning --error-exitcode=1 \
         components/ main/ --exclude=third_party/ --exclude=managed_components/
   ```

2. Consider `clone-detect` or `simian` for duplicate code detection
3. Document duplication thresholds in CONTRIBUTING.md

## References
- [Cppcheck](http://cppcheck.sourceforge.net/)
- [ESP-IDF Static Analysis](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/static-analysis.html)
