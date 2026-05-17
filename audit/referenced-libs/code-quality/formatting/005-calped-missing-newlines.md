---
title: "[LOW] Missing final newline in CalEPD external library files"
severity: LOW
domain: code-quality/formatting
lens: formatting-consistency
labels:
  - "audit:code-quality/formatting"
---

## Summary
The CalEPD external library component contains files missing a final newline at the end. POSIX standard requires text files to end with a newline character.

**Affected files:** Approximately 192 files in `components/CalEPD/`

**Examples of affected files:**
- `components/CalEPD/include/gdep015OC1.h`
- `components/CalEPD/include/gdew_colors.h`
- `components/CalEPD/include/gdew075HD.h`
- `components/CalEPD/include/parallel/grayscales.h`
- `components/CalEPD/models/wave12i48.cpp`
- And ~187 more files...

## Impact
- **POSIX compliance**: Text files should end with a newline (POSIX standard)
- **Tool compatibility**: Some tools (diff, cat, etc.) may behave unexpectedly
- **Diff noise**: When editing files, missing newline creates "No newline at end of file" warnings

## Evidence

**File: `components/CalEPD/include/gdew_colors.h`**
File ends without a newline character:
```
0x00F800    // No newline at end
```

## Recommended Fix

Since CalEPD is an external library, you have several options:

1. **Option 1 (Recommended)**: Add final newlines to all CalEPD files:
   ```bash
   find components/CalEPD -type f \( -name "*.h" -o -name "*.cpp" \) | while read f; do
       # Add newline if not present
       [ -n "$(tail -c 1 "$f")" ] && echo "" >> "$f"
   done
   ```

2. **Option 2**: Use a formatter configuration (`.editorconfig`) to ensure final newline:
   ```ini
   [*.{h,cpp,c}]
   insert_final_newline = true
   ```

3. **Option 3**: Add a pre-commit hook that ensures final newlines

## References
- POSIX standard: Text files should end with a newline
- C++ style guides: Most require final newline for consistency
