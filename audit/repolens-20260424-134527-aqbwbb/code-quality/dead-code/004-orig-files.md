---
title: "[MEDIUM] Remove orphaned .orig backup files"
severity: MEDIUM
domain: code-quality
lens: dead-code
labels:
  - "audit:code-quality/dead-code"
---

## Summary
The repository contains **2 `.orig` backup files** in the CalEPD component. These are typically created when applying patches or running `diff` commands and contain old versions of source files. They are dead code - not included in the build and serve no purpose.

**Affected files:**
1. `/input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/epdspi.cpp.orig` (6,355 bytes)
2. `/input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/._epdspi.cpp.orig` (macOS dotfile)

## Impact
- **Repository bloat**: ~6KB of dead code
- **Confusion for developers**: What is the difference between `.cpp` and `.cpp.orig`?
- **Version control noise**: Appear in `git status`, `git diff`
- **Maintenance burden**: These files need to be tracked but aren't part of the build

## Evidence
```bash
# Find all .orig files
$ find /input/20260423-132359-oj8ayc/cdc-badge-os -name "*.orig" -type f
/input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/epdspi.cpp.orig
/input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/._epdspi.cpp.orig

# Compare with current file
$ diff /input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/epdspi.cpp /input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/epdspi.cpp.orig
# (Files are different - orig is an older version)
```

The `.orig` file contains an older version of `epdspi.cpp` with different initialization code:
```cpp
// Old version in .orig file:
void EpdSpi::init(uint8_t frequency=4,bool debug=false){
    debug_enabled = debug;
    // ...
}

// Current version uses different signature:
void EpdSpi::init(uint8_t frequency, bool debug) {
    // ...
}
```

## Recommended Fix
1. **Remove the .orig files**:
   ```bash
   rm /input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/epdspi.cpp.orig
   rm /input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/._epdspi.cpp.orig
   ```

2. **Commit the removal**:
   ```bash
   git add -u
   git commit -m "chore: Remove orphaned .orig backup files"
   ```

3. **Add to `.gitignore` to prevent future occurrences**:
   ```
   # Backup and patch files
   *.orig
   *.bak
   *.swp
   *~
   ```

## References
- [Git documentation on ignored files](https://git-scm.com/docs/gitignore)
- [Best practices for removing dead code](https://www.agilealliance.org/glossary/dead-code/)
