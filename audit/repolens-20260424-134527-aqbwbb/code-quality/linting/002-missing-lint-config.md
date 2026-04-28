---
title: "[MEDIUM] Missing centralized C/C++ linting configuration and CI integration"
severity: MEDIUM
domain: linting
lens: code-quality/linting
labels:
  - "audit:code-quality/linting"
---

## Summary
The project lacks a centralized C/C++ linting configuration that can be enforced in CI. While a `.clang-format` exists in `third_party/libtropic/`, there is:
1. No root-level `.clang-format` for the main codebase
2. No `.clang-tidy` configuration for static analysis
3. No lint step in the GitHub Actions CI pipeline
4. No pre-commit hook configuration

**Current state:**
- `.clang-format` exists only in `third_party/libtropic/.clang-format` (for external library)
- No `.clang-tidy` configuration found anywhere in the repository
- `build.yml` CI only runs `pio build`, no linting step
- No pre-commit hooks configured (only `.git/hooks/pre-commit.sample` exists)

## Impact
1. **Inconsistent code style**: Without root `.clang-format`, developers may use different formatting styles
2. **Missed static analysis**: No `.clang-tidy` means common C++ issues (unused variables, potential null dereferences, etc.) are not caught automatically
3. **No CI enforcement**: Code quality checks run locally (if at all) but are not enforced in CI
4. **Third-party config only**: The existing `.clang-format` is in `third_party/libtropic/` and doesn't apply to main codebase

## Evidence

**Missing files (should exist at root):**
- `.clang-format` - Code style configuration
- `.clang-tidy` - Static analysis configuration  
- `.pre-commit-config.yaml` - Pre-commit hook configuration

**CI workflow (`build.yml`):**
```yaml
# Lines 31-35
- name: Build firmware
  run: pio build
# No lint step before or after build
```

**Existing config (wrong location):**
- `third_party/libtropic/.clang-format` - Only applies to external library

## Recommended Fix

### Immediate (1 hour)
1. **Create root `.clang-format`** based on the existing one in `third_party/libtropic/`:
   ```bash
   cp third_party/libtropic/.clang-format .clang-format
   ```
   Or create a new one with consistent style for the project.

2. **Add lint step to CI** (`build.yml`):
   ```yaml
   - name: Run clang-format check
     run: |
       find components main -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run --Werror
   ```

### Short-term (1-2 hours)
3. **Create `.clang-tidy`** configuration:
   ```yaml
   Checks: >-
     *-decl,
     *-def,
     *-init,
     modernize-*
   ```

4. **Add pre-commit hook** (`.pre-commit-config.yaml`):
   ```yaml
   repos:
     - repo: https://github.com/pre-commit/mirrors-clang-format
       rev: v14.0.0
       hooks:
         - id: clang-format
   ```

### Medium-term
5. Consider adding `cpplint` or `clang-tidy` to CI pipeline
6. Document linting setup in `README.md` or `CLAUDE.md`

## References
- clang-format documentation: https://clang.llvm.org/docs/ClangFormat.html
- clang-tidy documentation: https://clang.llvm.org/extra/clang-tidy/
- ESP-IDF coding style guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/contribute/coding-style.html
