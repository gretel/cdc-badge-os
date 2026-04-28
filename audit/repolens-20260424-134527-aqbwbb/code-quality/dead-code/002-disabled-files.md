---
title: "[MEDIUM] Remove orphaned .disabled files from source tree"
severity: MEDIUM
domain: code-quality
lens: dead-code
labels:
  - "audit:code-quality/dead-code"
---

## Summary
The repository contains **2 orphaned `.disabled` files** that are old versions of source files that were disabled during development but never properly removed. These files are dead code - they are not included in the build and serve no purpose.

**Affected files:**
1. `/input/20260423-132359-oj8ayc/cdc-badge-os/components/cdc_hal/src/EpaperDisplay.cpp.disabled` (8,237 bytes)
2. `/input/20260423-132359-oj8ayc/cdc-badge-os/components/cdc_hal/src/._EpaperDisplay.cpp.disabled` (macOS dotfile)

## Impact
- **Repository bloat**: ~8KB of dead code
- **Confusion for developers**: What does `.disabled` mean? Is it still used?
- **Maintenance burden**: These files need to be tracked but aren't part of the build
- **Version control noise**: Appear in `git status`, `git log`

## Evidence
The `.disabled` file contains an older version of `EpaperDisplay.cpp`:
```bash
$ find /input/20260423-132359-oj8ayc/cdc-badge-os -name "*.disabled" -type f
/input/20260423-132359-oj8ayc/cdc-badge-os/components/cdc_hal/src/EpaperDisplay.cpp.disabled
/input/20260423-132359-oj8423-132359-oj8ayc/cdc-badge-os/components/cdc_hal/src/._EpaperDisplay.cpp.disabled

$ wc -l /input/20260423-132359-oj8ayc/cdc-badge-os/components/cdc_hal/src/EpaperDisplay.cpp.disabled
244 /input/20260423-132359-oj8ayc/cdc-badge-os/components/cdc_hal/src/EpaperDisplay.cpp.disabled
```

The file contains a complete but unused implementation of `EpaperDisplay` class. The current active version is `EpaperDisplay.cpp` (15,271 bytes, 300+ lines).

## Recommended Fix
1. **Remove the disabled files**:
   ```bash
   rm /input/20260423-132359-oj8ayc/cdc-badge-os/components/cdc_hal/src/EpaperDisplay.cpp.disabled
   rm /input/20260423-132359-oj8ayc/cdc-badge-os/components/cdc_hal/src/._EpaperDisplay.cpp.disabled
   ```

2. **If the code is needed for reference, add it to git history properly**:
   ```bash
   # Find when it was disabled
   git log --all --oneline -- "**/EpaperDisplay.cpp.disabled"
   
   # If needed, create a tag or branch for historical reference
   git tag epaper-display-old HEAD
   ```

3. **If the code is no longer needed, commit the removal**:
   ```bash
   git add -u
   git commit -m "chore: Remove orphaned .disabled files"
   ```

4. **Add to `.gitignore` to prevent future occurrences**:
   ```
   # Backup and disabled files
   *.disabled
   *.bak
   *.orig
   ```

## References
- [Git documentation on ignored files](https://git-scm.com/docs/gitignore)
- [Best practices for removing dead code](https://www.agilealliance.org/glossary/dead-code/)
