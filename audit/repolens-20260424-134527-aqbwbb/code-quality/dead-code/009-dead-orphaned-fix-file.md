---
title: "[MEDIUM] Dead Code: Orphaned file in CalEPD fix directory"
severity: MEDIUM
domain: dead-code
lens: code-quality
labels:
  - "audit:code-quality/dead-code"
---

## Summary
There is an orphaned source file in the `components/CalEPD/models/fix/` directory that is not included in the build system and not referenced anywhere in the codebase.

**Location:** `components/CalEPD/models/fix/gdeh0213b73.cpp`

This file appears to be a modified version of `components/CalEPD/models/gdeh0213b73.cpp` that was placed in a `fix/` subdirectory but never integrated into the build.

## Impact
- **Confusion**: Developers may wonder if this is the correct version or if it should replace the main file
- **Maintenance burden**: Two versions of the same file need to be maintained
- **Build consistency**: The fix directory is not referenced in any CMakeLists.txt

## Evidence
File: `components/CalEPD/models/fix/gdeh0213b73.cpp`

Key differences from the main file (`components/CalEPD/models/gdeh0213b73.cpp`):
1. Missing includes (`stdint.h`, `stdbool.h`, `inttypes.h`)
2. Missing `#define GDEH0213B73_PU_DELAY 300`
3. Different LUT command value (`0x21` vs `0x32`)
4. Different function call syntax (`IO.data(data, databytes)` vs loop)
5. Missing `_wakeUp()` and `cmd(0x2C)` calls

The fix directory is not referenced in any CMakeLists.txt or included anywhere:
```bash
$ grep -rn "fix/gdeh0213b73\|models/fix" /input/20260423-132359-oj8ayc/cdc-badge-os --include="*.cpp" --include="*.h" --include="CMakeLists.txt"
# (no results)
```

## Recommended Fix
Choose one of the following approaches:

1. **If the fix is correct and should be used**:
   - Merge the changes into the main file
   - Delete the `fix/` directory

2. **If the main file is correct**:
   - Delete `components/CalEPD/models/fix/gdeh0213b73.cpp`
   - Optionally delete the empty `fix/` directory

3. **If the fix is experimental**:
   - Move it to a `test/` or `experimental/` directory
   - Or add a clear comment explaining its purpose

## References
- Dead file detection best practices
- Build system organization
