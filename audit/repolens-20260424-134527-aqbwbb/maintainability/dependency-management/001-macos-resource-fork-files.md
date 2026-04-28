---
title: "[MEDIUM] macOS Resource Fork Files (._*) Polluting Repository"
severity: MEDIUM
domain: dependency-management
lens: maintainability
labels:
  - "clean-up"
  - "macos"
  - "git"
---

## Summary

The repository contains **8,771 macOS resource fork files** (files prefixed with `._`) scattered across the codebase. These are metadata files created by macOS when copying files to different filesystems and should be excluded from version control.

**Affected locations:**
- Root directory: `._.`, `._web-flasher`, `._LICENSE.md`, `._CMakeLists.txt`
- Tools directory: `._requirements.txt`, `._ble_serial.py`, `._flash_firmware.py`
- Web flasher: `._index.html`, `._badge.jpg`
- Build artifacts: `._.pio`, `._.DS_Store`
- Documentation: `docs/._.DS_Store`, `doxygen_output/._.DS_Store`
- Dependencies: Multiple files in `third_party/libtropic/` and `components/`

**Evidence:**
```bash
$ find . -name "._*" -type f | wc -l
8771

$ find . -name "._*" -type f | head -10
./._.
./._web-flasher
./web-flasher/._index.html
./tools/._requirements.txt
./components/CalEPD/._.git
./third_party/libtropic/._.git
```

**Current .gitignore** (line 1-3):
```
# macOS
.DS_Store
Thumbs.db
```

The `.gitignore` only excludes `.DS_Store` but **not** the `._*` resource fork files.

## Impact

1. **Repository Bloat**: 8,771 extra files increase clone/pull times and disk usage unnecessarily
2. **CI/CD Noise**: Build workflows may process these files, adding overhead
3. **Confusion**: Developers may accidentally commit or reference these files
4. **Cross-platform issues**: Non-macOS developers see unexpected files
5. **Build artifacts included**: Files like `._.pio` and `._build` should be ignored entirely

## Recommended Fix

**Update `.gitignore` to exclude macOS resource fork files:**

```diff
 # macOS
 .DS_Store
+._*
 Thumbs.db
```

Alternatively, use this more specific pattern to only match resource forks:

```diff
 # macOS
 .DS_Store
 Thumbs.txt
+AppleDouble files
+._*
```

**Cleanup existing files:**

Run this command to remove all existing `._*` files:
```bash
find . -name "._*" -type f -delete
```

Then commit the `.gitignore` change and the deletion.

**Optional: Configure macOS to not create these files:**

For future reference, on macOS run:
```bash
defaults write com.apple.desktopservices DSDontWriteNetworkStores true
```

## References

- [AppleDouble file format](https://en.wikipedia.org/wiki/AppleDouble)
- [gitignore templates - macOS](https://github.com/github/gitignore/blob/main/macOS.gitignore)
- [Stack Overflow: How to ignore ._ files in git](https://stackoverflow.com/questions/7241504/how-to-ignore-_-files-in-git)
