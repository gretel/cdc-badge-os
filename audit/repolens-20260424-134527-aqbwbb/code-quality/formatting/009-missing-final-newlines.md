---
title: "[LOW] Missing final newline at end of file in main codebase"
severity: LOW
domain: code-quality/formatting
lens: formatting-consistency
labels:
  - "audit:code-quality/formatting"
---

## Summary
Many files in the main codebase are missing a final newline at the end of the file. POSIX standard requires text files to end with a newline character.

**Affected files:** 80+ files in the main codebase (excluding CalEPD)

**Examples of affected files:**
- `components/cdc_core/include/cdc_core/ServiceRegistry.h`
- `components/cdc_core/src/ServiceRegistry.cpp`
- `components/cdc_hal/include/cdc_hal/IDisplay.h`
- `components/cdc_hal/src/TCA9535Keypad.cpp`
- `components/cdc_ui/src/I18n.cpp`
- `components/cdc_os_ui/src/AppUi.cpp`
- `components/mod_totp/include/mod_totp/TotpModule.h`
- `components/mod_totp/src/TotpModule.cpp`
- `components/usb_badge/usb_cdc.cpp`
- `main/main.cpp`
- And 70+ more files...

## Impact
- **POSIX compliance**: Text files should end with a newline (POSIX standard)
- **Tool compatibility**: Some tools (diff, cat, etc.) may show "No newline at end of file" warnings
- **Diff noise**: When editing files, missing newline creates warnings in diffs
- **Concatenation issues**: When concatenating files, the last line of one file may merge with the first line of the next

## Evidence

**File: `components/mod_totp/src/TotpModule.cpp`**
File ends without a newline:
```cpp
    });
}  ← No newline after closing brace
```

**File: `components/cdc_core/include/cdc_core/ServiceRegistry.h`**
File ends without a newline:
```cpp
};  ← No newline after closing brace
```

**Count of affected files:**
- cdc_core: 20 files
- cdc_hal: 20 files
- cdc_ui: 5 files
- cdc_os_ui: 17 files
- mod_totp: 4 files
- usb_badge: 5 files
- main: 2 files
- include: 1 file
- Total: ~80 files

## Recommended Fix

**Option 1 (Recommended)**: Add final newlines to all files:
```bash
# Add newline to end of all C/C++ files in main codebase
find components/cdc_core components/cdc_hal components/cdc_ui components/cdc_os_ui components/mod_totp components/usb_badge main include -type f \( -name "*.h" -o -name "*.cpp" -o -name "*.c" \) | while read f; do
    # Add newline if not present
    [ -n "$(tail -c 1 "$f")" ] && echo "" >> "$f"
done
```

**Option 2**: Use editorconfig to ensure final newline:
Create `.editorconfig` at project root:
```ini
[*.{h,cpp,c}]
insert_final_newline = true
```

**Option 3**: Add pre-commit hook:
```bash
# .git/hooks/pre-commit
#!/bin/bash
git diff --cached --name-only | grep -E '\.(h|cpp|c)$' | while read f; do
    [ -n "$(tail -c 1 "$f")" ] && echo "" >> "$f"
    git add "$f"
done
```

**Option 4**: Use sed to add newlines:
```bash
find components/cdc_core components/cdc_hal components/cdc_ui components/cdc_os_ui components/mod_totp components/usb_badge main include -type f \( -name "*.h" -o -name "*.cpp" -o -name "*.c" \) -exec sed -i -e '$a\' {} \;
```

## References
- POSIX standard: Text files should end with a newline
- C++ style guides: Most require final newline for consistency
- EditorConfig specification: https://editorconfig.org/
