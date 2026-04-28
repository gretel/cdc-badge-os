---
title: "[LOW] Trailing whitespace in CalEPD external library files"
severity: LOW
domain: code-quality/formatting
lens: formatting-consistency
labels:
  - "audit:code-quality/formatting"
---

## Summary
The CalEPD external library component contains trailing whitespace in many files. While this is an external dependency, cleaning it up would reduce diff noise and improve consistency.

**Affected files:** Approximately 248 files in `components/CalEPD/`

**Examples of affected files:**
- `components/CalEPD/include/epdParallel.h` (3 lines with trailing whitespace)
- `components/CalEPD/include/gdep015OC1.h`
- `components/CalEPD/include/gdew_colors.h`
- `components/CalEPD/include/gdew075HD.h`
- `components/CalEPD/epdParallel.cpp`
- `components/CalEPD/epdspi.cpp`
- And ~240 more files...

## Impact
- **Diff noise**: Trailing whitespace creates unnecessary changes when files are edited
- **Consistency**: The main project code (excluding CalEPD) has no trailing whitespace

## Evidence

**File: `components/CalEPD/include/epdParallel.h`**
Lines 20, 28, 39 contain trailing whitespace (spaces at end of line):
```cpp
    const char* TAG = "Epd driver I2S DataBus";
    // ... (line ends with spaces)
```

## Recommended Fix

Since CalEPD is an external library, you have several options:

1. **Option 1 (Recommended)**: Add trailing whitespace stripping to your git pre-commit hook or CI:
   ```bash
   # Add to .gitattributes or a pre-commit script
   find components/CalEPD -type f \( -name "*.h" -o -name "*.cpp" \) -exec sed -i 's/[[:space:]]*$//' {} \;
   ```

2. **Option 2**: Use a text editor command to strip all trailing whitespace:
   ```bash
   find components/CalEPD -type f \( -name "*.h" -o -name "*.cpp" \) | while read f; do
       sed -i 's/[[:space:]]*$//' "$f"
   done
   ```

3. **Option 3**: Add a formatter configuration (`.editorconfig`) that automatically strips trailing whitespace:
   ```ini
   [*.{h,cpp,c}]
   trim_trailing_whitespace = true
   ```

## References
- `.editorconfig` specification: https://editorconfig.org/
- Common practice: Strip trailing whitespace in C/C++ projects
