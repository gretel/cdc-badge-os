---
title: "[MEDIUM] Remove macOS resource fork dotfiles (._*) from source tree"
severity: MEDIUM
domain: code-quality
lens: dead-code
labels:
  - "audit:code-quality/dead-code"
---

## Summary
The repository contains **1,723 macOS resource fork dotfiles** (files prefixed with `._`) scattered throughout the source tree. These are metadata files created by macOS when copying files to cross-platform filesystems and contain no actual source code. They are dead files that serve no purpose in the build process and clutter the repository.

**Affected locations (excluding build directory):**
- `components/CalEPD/` - ~300+ files
- `components/mod_totp/` - 8 files
- `components/mod_fido2/` - 10+ files
- `components/mod_gpg/` - 10+ files
- `components/cdc_hal/` - 20+ files
- `components/usb_badge/` - 6 files
- `test/` - 3 test files
- `third_party/` - 500+ files
- `include/` - 1 file

**Example files:**
- `/input/20260423-132359-oj8ayc/cdc-badge-os/test/test_vcard_module_link/._test_vcard_module_link.cpp`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_totp/include/mod_totp/._TotpStore.h`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/include/._epdParallel.h`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/include/._tusb_config.h`

## Impact
- **Repository bloat**: ~1,723 unnecessary files increasing clone/pull times
- **Build system confusion**: Some build systems may attempt to process these files
- **Version control noise**: These files appear in `git status`, `git diff`, etc.
- **Cross-platform confusion**: Developers on Linux/Windows may not understand what these files are

## Evidence
```bash
# Count of macOS dotfiles (excluding .pio/build which is a build artifact)
$ find /input/20260423-132359-oj8ayc/cdc-badge-os -name ".*.cpp" -o -name ".*.h" -o -name ".*.c" | grep -v ".pio/build" | wc -l
1723

# Sample of affected files
$ find /input/20260423-132359-oj8ayc/cdc-badge-os -name ".*.cpp" -o -name ".*.h" | grep -v ".pio/build" | head -10
/input/20260423-132359-oj8ayc/cdc-badge-os/test/test_vcard_module_link/._test_vcard_module_link.cpp
/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_totp/include/mod_totp/._TotpStore.h
/input/20260423-132359-oj8ayc/cdc-badge-os/components/mod_fido2/src/._Fido2Module.cpp
```

These files are typically 100-200 bytes each, containing AppleDouble format metadata (resource forks, Finder info, extended attributes).

## Recommended Fix
1. **Remove all existing dotfiles**:
   ```bash
   find . -name ".*" -type f | grep -v ".git" | grep -v ".pio/build" | xargs rm -f
   ```

2. **Add to `.gitattributes`** to prevent future occurrences:
   ```
   # macOS resource forks - keep them out of git
   .DS_Store              text=auto eol=lf
   **/.*                  text=auto eol=lf
   ```

3. **Add to `.gitignore`** (optional, for local Mac development):
   ```
   # macOS files
   .DS_Store
   ._*
   ```

4. **Commit the cleanup**:
   ```bash
   git add -u
   git commit -m "chore: Remove macOS resource fork dotfiles (._*) from source tree"
   ```

## References
- [macOS AppleDouble format](https://en.wikipedia.org/wiki/AppleDouble)
- [Git attributes documentation](https://git-scm.com/docs/gitattributes)
- [How to ignore macOS .DS_Store and dotfiles](https://www.git-tower.com/blog/make-git-reappearance-clean/)
