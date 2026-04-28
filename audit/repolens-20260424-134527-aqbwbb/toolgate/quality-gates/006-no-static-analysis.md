---
title: "[MEDIUM] No static analysis (clang-tidy) configured"
severity: MEDIUM
domain: code-quality
lens: quality-gates
labels:
  - "audit:toolgate/quality-gates"
  - "static-analysis"
---

## Summary
The project lacks clang-tidy for static analysis to catch common C++ bugs, style issues, and modern C++ best practices.

**Evidence:**
- No `.clang-tidy` configuration file
- No static analysis step in `.github/workflows/build.yml`
- 764 C/C++ files without automated static analysis

## Impact
- Common C++ bugs may go undetected (null pointer dereference, uninitialized variables, etc.)
- Missed opportunities for modern C++ improvements
- No automated check for RAII, smart pointers, const correctness
- Memory issues in embedded context harder to catch early

## Recommended Fix
1. Create `.clang-tidy` configuration at repository root:
   ```yaml
   Checks: >-
     -bugprone-*,
     -bugprone-easily-swappable-default-arguments,
     -clang-analyzer-alpha.*,
     -cert-*,
     -cppcoreguidelines-avoid-magic-numbers,
     -modernize-pass-by-value,
     -readability-function-cognitive-complexity
   ```

2. Add CI step (note: may need to run on ESP-IDF build output):
   ```yaml
   - name: Run clang-tidy
     run: |
       # Requires ESP-IDF environment setup
       idf.py build -T clang-tidy
   ```

3. Consider `esp-tidy` (ESP-IDF specific static analysis tool)

## References
- [clang-tidy documentation](https://clang.llvm.org/extra/clang-tidy/)
- [ESP-IDF Static Analysis](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/static-analysis.html)
- [Modern C++ Checklist](https://clang.llvm.org/extra/clang-tidy/checks/list.html)
