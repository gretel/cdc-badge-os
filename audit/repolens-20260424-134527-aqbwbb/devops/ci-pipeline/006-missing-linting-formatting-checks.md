---
title: "[LOW] Missing linting and formatting checks in CI pipeline"
severity: LOW
domain: devops
lens: ci-pipeline
labels:
  - "audit:devops/ci-pipeline"
---

## Summary
The CI pipeline lacks linting and formatting checks. The codebase has established style guidelines (Doxygen-style comments, C++17, specific naming conventions), but there's no automated enforcement in CI.

**Evidence:**
- File: `.github/workflows/build.yml` - Lines 1-90
- No `clang-format`, `uncrustify`, or similar tool
- No linting step for C++ code

## Impact
- Inconsistent code style across the codebase
- Manual review required for style compliance
- Style drift over time as new developers join
- Larger diffs due to formatting changes mixed with logic changes

## Evidence
```yaml
# build.yml has no formatting/linting step
- name: Build firmware
  run: pio run
# No clang-format, no style check
```

Style guidelines from CLAUDE.md:
- Doxygen-style comments with backslash commands (`\brief`, `\param`, `\return`)
- C++17 standard
- Class-based design, DRY, single responsibility

## Recommended Fix
Add a formatting check step:

```yaml
- name: Install clang-format
  run: sudo apt-get install -y clang-format

- name: Check formatting
  run: |
    find components/ main/ -name "*.cpp" -o -name "*.h" | \
      xargs clang-format --dry-run --Werror
```

Alternatively, use a pre-commit hook with `pre-commit` framework:
1. Create `.pre-commit-config.yaml`
2. Add `clang-format`, `check-merge-conflicts`, etc.

## References
- [clang-format](https://clang.llvm.org/docs/ClangFormat.html)
- [pre-commit framework](https://pre-commit.com/)
- [ESP-IDF Coding Style](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/contribute/coding-style.html)
