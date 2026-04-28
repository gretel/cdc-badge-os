---
title: "[MEDIUM] Missing .clang-format configuration at project root"
severity: MEDIUM
domain: code-quality/formatting
lens: formatting-consistency
labels:
  - "audit:code-quality/formatting"
---

## Summary
The project lacks a `.clang-format` configuration file at the root level. While a `.clang-format` exists in `third_party/libtropic/`, it only applies to that subdirectory. Without a root-level configuration, there is no enforced formatting standard for the main codebase.

**Current state:**
- `third_party/libtropic/.clang-format` exists (applies only to libtropic)
- `managed_components/espressif__tinyusb/.clang-format` exists (external dependency)
- **Root directory has no `.clang-format`**

## Impact
- **Inconsistent formatting**: Developers must manually decide on formatting style for each file
- **PR noise**: Pull requests may contain unnecessary formatting changes
- **Onboarding friction**: New developers need to guess the preferred formatting style
- **No CI enforcement**: Without a config file, CI cannot auto-format or check formatting consistency

## Evidence

**Existing formatter config (only in third_party):**
`third_party/libtropic/.clang-format`:
```yaml
BasedOnStyle: Google
BreakBeforeBinaryOperators: All
ColumnLimit: 120
BreakBeforeBraces: Stroustrup
IndentWidth: 4
PointerAlignment: Right
```

**Main codebase files without formatter guidance:**
- `components/cdc_core/src/ServiceRegistry.cpp`
- `components/cdc_hal/src/TCA9535Keypad.cpp`
- `components/mod_totp/src/TotpModule.cpp`
- `components/cdc_os_ui/src/views/LockScreenView.cpp`
- `main/main.cpp`
- And 300+ other C/C++ files...

**Observed inconsistencies:**
1. CalEPD uses tabs for indentation (29+ files)
2. Main codebase uses 4-space indentation
3. No centralized formatting rules for new contributors

## Recommended Fix

**Option 1 (Recommended)**: Add a `.clang-format` file at the project root:

Create `/input/20260423-132359-oj8ayc/cdc-badge-os/.clang-format`:
```yaml
BasedOnStyle: Google
IndentWidth: 4
TabWidth: 4
UseTab: Never
ColumnLimit: 120
BreakBeforeBraces: Stroustrup
PointerAlignment: Right
```

**Option 2**: Apply formatting to existing code:
```bash
# After adding .clang-format
find components main include -type f \( -name "*.h" -o -name "*.cpp" -o -name "*.c" \) | xargs clang-format -i
```

**Option 3**: Add pre-commit hook to auto-format staged files:
```bash
# .git/hooks/pre-commit
#!/bin/bash
git diff --cached --name-only | grep -E '\.(h|cpp|c)$' | xargs clang-format -i
git add $(git diff --cached --name-only | grep -E '\.(h|cpp|c)$')
```

**Option 4**: Add formatter check to CI workflow.

## References
- Clang-Format documentation: https://clang.llvm.org/docs/ClangFormat.html
- Google C++ Style Guide: https://google.github.io/styleguide/cpp.html
- `.clang-format` configuration options: https://releases.llvm.org/14.0.0/docs/ClangFormatStyleOptions.html
- Common indentation styles: K&R, Allman, Stroustrup, Google, LLVM
