---
title: "[HIGH] macOS resource fork files (._*) cluttering components directory"
severity: HIGH
domain: maintainability
lens: tech-debt/cleanup
labels:
  - cleanup
  - build-system
---

## Summary
Over 540 macOS resource fork files (prefixed with `._`) are present in the `components/` directory, polluting the source tree and potentially causing build issues.

## Impact
- **Repository bloat**: 540+ unnecessary files increase repository size
- **Build confusion**: Build systems may process these as source files
- **Cross-platform issues**: Causes problems for non-macOS developers
- **Git noise**: Creates unnecessary diff noise and commit history
- **CMake issues**: May interfere with glob patterns in CMakeLists.txt

## Evidence

### Sample of affected files in components directory:
```
components/._mod_totp
components/mod_totp/._CMakeLists.txt
components/mod_totp/._src/._TotpModule.cpp
components/._CalEPD
components/CalEPD/._epdspi.cpp.orig
components/CalEPD/epdspi.cpp.orig
components/mod_gpg/src/openpgp/._apdu.cpp
components/mod_gpg/src/openpgp/._openpgp.cpp
components/mod_totp/include/mod_totp/._TotpStore.h
```

Total count: 540 files with `._` prefix in components/

### Additional backup files found:
```
components/CalEPD/epdspi.cpp.orig
```

## Recommended Fix

### Step 1 - Create a .gitattributes file:
Add to repository root:
```gitattributes
# Ignore macOS resource forks
._*    text eol=lf
```

### Step 2 - Remove existing resource forks:
Run from repository root:
```bash
# Find and list all resource fork files
find components -name "._*" -type f > /tmp/resource_forks.txt

# Remove them
find components -name "._*" -type f -delete

# Also remove .orig backup files
find components -name "*.orig" -type f -delete
```

### Step 3 - Add to .gitignore:
Add to `.gitignore`:
```gitignore
# macOS resource forks
._*

# Editor backup files
*.orig
*.swp
*.swo
```

### Step 4 - Commit cleanup:
```bash
git add -A
git commit -m "cleanup: remove macOS resource fork files from components"
```

### Step 5 - Prevent future occurrences:
Add to repository documentation (CLAUDE.md or CONTRIBUTING.md):
```markdown
### macOS Users
Run `xattr -cr <file>` before adding files to prevent resource forks.
Or configure git to strip them automatically.
```

## References
- macOS resource forks: https://en.wikipedia.org/wiki/Resource_fork
- Git attributes: https://git-scm.com/docs/gitattributes
- Common .gitignore patterns: https://github.com/github/gitignore
