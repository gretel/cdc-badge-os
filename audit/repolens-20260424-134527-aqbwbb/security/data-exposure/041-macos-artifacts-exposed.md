---
title: "[MEDIUM] macOS Resource Fork Files and .DS_Store Artifacts Exposed in Repository"
severity: MEDIUM
domain: Data Exposure
lens: file-exposure
labels:
  - "audit:security/data-exposure"
  - "macos-artifacts"
---

## Summary
The repository contains approximately **8,775 macOS-specific metadata files** (`.DS_Store` and `._*` resource fork files) scattered throughout the codebase. These files are macOS filesystem artifacts that can leak local filesystem information and clutter the repository.

**Location**: Throughout the repository root and subdirectories

**Evidence**:
```bash
# Count of macOS artifacts found:
find /input/20260423-132359-oj8ayc/cdc-badge-os -name ".DS_Store" -o -name "._*" | wc -l
# Result: 8775 files

# Examples of exposed artifacts:
- /input/20260423-132359-oj8ayc/cdc-badge-os/.DS_Store
- /input/20260423-132359-oj8ayc/cdc-badge-os/._.DS_Store
- /input/20260423-132359-oj8ayc/cdc-badge-os/._web-flasher
- /input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/._index.html
- /input/20260423-132359-oj8ayc/cdc-badge-os/tools/._requirements.txt
- /input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/._Kconfig.projbuild
```

## Impact

**Information Leakage**:
- `.DS_Store` files contain directory listing information (file names, modification dates, icons)
- `._*` resource fork files contain extended attributes and metadata about original files
- Can reveal local filesystem structure, file organization, and development environment details
- Clutters Git repository with unnecessary binary data

**Repository Bloat**:
- 8,775+ unnecessary files increase repository size
- Slows down clone/pull operations
- Makes repository navigation more difficult

**Professional Presentation**:
- Indicates incomplete `.gitignore` configuration
- Suggests macOS developers not following best practices for cross-platform development

## Evidence

**Current .gitignore** (limited coverage):
```
# macOS
.DS_Store
Thumbs.db
```

**Missing from .gitignore**:
- `._*` (AppleDouble resource fork files)
- `.DS_Store` files in subdirectories (glob pattern needed)

**Example .DS_Store content** (reveals directory structure):
```
Mac OS X        1   2   9   F   2   x   @   ATTR
```

## Recommended Fix

### 1. Update .gitignore (5 minutes)
Add comprehensive macOS artifact patterns to `.gitignore`:

```gitignore
# macOS
.DS_Store
.DS_Store?
._*
.Spotlight-V100
.Trashes
```

### 2. Remove existing artifacts (10 minutes)
Run cleanup command from repository root:

```bash
# Find and remove all .DS_Store and ._* files
find . -type f -name ".DS_Store" -exec rm -f {} +
find . -type f -name "._*" -exec rm -f {} +

# Alternative one-liner:
find . -depth -maxdepth 100 -type f \( -name ".DS_Store" -o -name "._*" \) -print | xargs rm -f
```

### 3. Commit the cleanup (5 minutes)
```bash
git add .gitignore
git add -A  # Stage all deletions
git commit -m "chore: Remove macOS artifacts and update .gitignore"
```

### 4. Configure Git globally for future (optional, 5 minutes)
Add to `~/.gitconfig`:
```ini
[core]
    excludesfile = ~/.gitignore_global
```

Add to `~/.gitignore_global`:
```gitignore
.DS_Store
._*
```

## References

- [gitignore - macOS](https://github.com/github/gitignore/blob/main/Global/macOS.gitignore) - Official GitHub macOS gitignore
- [AppleDouble format](https://en.wikipedia.org/wiki/AppleDouble) - Resource fork file format
- [DS_Store information leakage](https://en.wikipedia.org/wiki/.DS_Store) - Wikipedia article
- Best practices for cross-platform Git repositories

## Related Issues

This finding is related to:
- Issue #22 (Build artifacts exposed in .pio/ directory)
- Issue #26 (compile_commands.json exposure)

All three address file exposure in the repository structure.
