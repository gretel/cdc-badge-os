---
title: "[MEDIUM] clang-format configuration exists but not enforced in CI"
severity: MEDIUM
domain: code-quality
lens: quality-gates
labels:
  - "audit:toolgate/quality-gates"
---

## Summary
A `clang-format` configuration file exists in `third_party/libtropic/.clang-format` (Google style, 120 column limit), but there is no CI workflow or script to enforce code formatting consistency across the main source code.

**Evidence:**
- Config file: `third_party/libtropic/.clang-format` (lines 1-13)
- Second config in: `managed_components/espressif__tinyusb/.clang-format` (LLVM style)
- No workflow in `.github/workflows/` runs `clang-format --check`
- No pre-commit hooks configured in project root

## Impact
- **Code consistency**: Different developers may use different formatting styles
- **Review burden**: PRs may contain formatting diffs unrelated to actual changes
- **Maintenance**: Harder to maintain consistent code style as the project grows

## Evidence
```yaml
# third_party/libtropic/.clang-format
BreakBeforeBinaryOperators: All
ColumnLimit: 120
BasedOnStyle: Google
BreakBeforeBraces: Stroustrup
IndentWidth: 4
PointerAlignment: Right
```

Current CI workflows (`.github/workflows/build.yml`):
- Only runs `pio run` (build check)
- No formatting validation step

## Recommended Fix
1. Add a `clang-format` check step to the build workflow:
```yaml
- name: Check code formatting
  run: |
    find components/ main/ -name "*.cpp" -o -name "*.h" | \
      xargs clang-format --dry-run --Werror
```

2. Alternatively, add a pre-commit hook:
```bash
# .pre-commit-config.yaml
repos:
  - repo: https://github.com/pre-commit/mirrors-clang-format
    rev: v14.0.0
    hooks:
      - id: clang-format
        files: \.(cpp|h)$
```

3. Document the formatting requirement in `CLAUDE.md` or `README.md`

## References
- [clang-format documentation](https://clang.llvm.org/docs/ClangFormat.html)
- [GitHub Actions clang-format example](https://github.com/marketplace/actions/clang-format-check)
- [Pre-commit clang-format hook](https://github.com/pre-commit/mirrors-clang-format)
