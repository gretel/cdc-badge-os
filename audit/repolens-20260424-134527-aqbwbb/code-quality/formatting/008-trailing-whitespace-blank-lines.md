---
title: "[LOW] Trailing whitespace on blank lines in main codebase"
severity: LOW
domain: code-quality/formatting
lens: formatting-consistency
labels:
  - "audit:code-quality/formatting"
---

## Summary
Many files in the main codebase have trailing whitespace on blank lines (lines that contain only spaces or tabs before the newline). This is distinct from trailing whitespace on content lines.

**Affected files:** 70+ files in the main codebase (excluding CalEPD)

**Examples of affected files:**
- `components/cdc_core/src/ServiceRegistry.cpp` (189 lines with trailing whitespace, mostly blank lines)
- `components/cdc_core/include/cdc_core/ServiceRegistry.h`
- `components/cdc_hal/src/TCA9535Keypad.cpp`
- `components/cdc_ui/src/I18n.cpp`
- `components/cdc_os_ui/src/AppUi.cpp`
- `components/mod_totp/src/TotpModule.cpp`
- `components/usb_badge/usb_cdc.cpp`
- `main/main.cpp`
- And 60+ more files...

## Impact
- **Diff noise**: When editing files, trailing whitespace on blank lines creates unnecessary diff changes
- **Editor warnings**: Many editors show "trailing whitespace" warnings
- **Consistency**: Clean codebases typically have no trailing whitespace on any lines

## Evidence

**File: `components/cdc_core/src/ServiceRegistry.cpp`**
Blank lines with trailing spaces:
```cpp
#include "cdc_core/ServiceRegistry.h"
#include "cdc_log.h"
#include <cstring>
   ← Line 4 has trailing spaces
static const char* TAG = "ServiceRegistry";
   ← Line 6 has trailing spaces
namespace cdc::core {
   ← Line 8 has trailing spaces
```

**Line-by-line analysis:**
- Total lines with trailing whitespace: 189
- Lines that are blank with trailing spaces: ~30
- Lines with content and trailing spaces: ~0 (mostly clean)

## Recommended Fix

**Option 1 (Recommended)**: Strip trailing whitespace from all files:
```bash
# Strip trailing whitespace from all C/C++ files in main codebase
find components/cdc_core components/cdc_hal components/cdc_ui components/cdc_os_ui components/mod_totp components/usb_badge main include -type f \( -name "*.h" -o -name "*.cpp" -o -name "*.c" \) | while read f; do
    sed -i 's/[[:space:]]*$//' "$f"
done
```

**Option 2**: Use editorconfig to automatically strip trailing whitespace:
Create `.editorconfig` at project root:
```ini
[*.{h,cpp,c}]
trim_trailing_whitespace = true
```

**Option 3**: Add pre-commit hook:
```bash
# .git/hooks/pre-commit
#!/bin/bash
git diff --cached --name-only | grep -E '\.(h|cpp|c)$' | while read f; do
    sed -i 's/[[:space:]]*$//' "$f"
    git add "$f"
done
```

**Option 4**: Use clang-format with `TrimTrailingWhitespace` enabled (requires CMakeLists or build system integration).

## References
- EditorConfig specification: https://editorconfig.org/
- Common practice: Strip trailing whitespace in C/C++ projects
- Git hooks documentation: https://git-scm.com/book/en/v2/Customizing-Git-Git-Hooks
