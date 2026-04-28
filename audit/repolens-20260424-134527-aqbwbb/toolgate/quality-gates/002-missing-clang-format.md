---
title: "[MEDIUM] No clang-format configuration for C/C++ code style"
severity: MEDIUM
domain: code-quality
lens: quality-gates
labels:
  - "audit:toolgate/quality-gates"
  - "formatting"
---

## Summary
The project has no `.clang-format` configuration file to enforce consistent C/C++ code style across the codebase.

**Evidence:**
- No `.clang-format` file found in repository root
- No clang-format check in GitHub Actions workflows (`.github/workflows/build.yml`)
- 764 C/C++ source files without a common formatting standard

## Impact
- Inconsistent code style across different modules and components
- Harder code reviews due to style variations
- Difficulty onboarding new developers
- Larger diffs in pull requests due to formatting changes
- The project emphasizes "Professional Code Only" (CLAUDE.md) but lacks enforcement

## Recommended Fix
1. Create a `.clang-format` file at repository root with ESP-IDF or Google style
2. Add a clang-format check step to the build workflow
3. Document the formatting standard in `CLAUDE.md` or a CONTRIBUTING file

**Example `.clang-format`:**
```yaml
BasedOnStyle: Google
IndentWidth: 4
TabWidth: 4
UseTab: Never
BreakBeforeBraces: Attach
PointerAlignment: Left
AccessModifierOffset: -4
FunctionArgumentsAfterTab: false
```

**CI addition:**
```yaml
- name: Check code formatting
  run: |
    find components/ main/ -name "*.h" -o -name "*.cpp" -o -name "*.c" | \
      xargs clang-format --dry-run --Werror
```

## References
- [clang-format documentation](https://clang.llvm.org/docs/ClangFormat.html)
- [ESP-IDF Coding Style](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/coding-style.html)
