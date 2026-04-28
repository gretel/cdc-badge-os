---
title: "[MEDIUM] macOS resource fork files cluttering CI workflow directory"
severity: MEDIUM
domain: devops
lens: ci-pipeline
labels:
  - "audit:devops/ci-pipeline"
---

## Summary
The `.github/workflows/` directory contains macOS resource fork files (`._build.yml` and `._deploy-pages.yml`) that are AppleDouble metadata files created by macOS Finder. These files are 163 bytes each and are not needed for CI execution but can cause confusion and clutter in version control.

**Evidence:**
- File: `.github/workflows/._build.yml` - 163 bytes, macOS metadata
- File: `.github/workflows/._deploy-pages.yml` - 163 bytes, macOS metadata
- These are hidden AppleDouble format files containing extended attributes

## Impact
- Clutters the workflow directory with non-functional files
- Can confuse developers unfamiliar with macOS file system behavior
- Files may be tracked in git if `.gitignore` doesn't exclude them
- Slight increase in repository size with each sync
- For a CI-focused repo, clean workflow directory is important

## Evidence
```bash
# macOS resource fork files present:
$ ls -la .github/workflows/
-rw-r--r--  1 user  staff  163 Jan 31 07:53 ._build.yml
-rw-r--r--  1 user  staff  163 Feb 25 14:05 ._deploy-pages.yml
-rw-r--r--  1 user  staff 2428 Jan 31 07:53 build.yml
-rw-r--r--  1 user  staff 4194 Feb 25 14:05 deploy-pages.yml

# File content shows AppleDouble format:
$ cat .github/workflows/._build.yml | od -c | head
0000000  \0 005 026  \a  \0 002  \0  \0   M   a   c       O   S       X
```

## Recommended Fix
1. **Remove the resource fork files:**
```bash
cd .github/workflows/
find . -name "._*" -type f -delete
```

2. **Add to `.gitignore` to prevent future occurrences:**
```gitignore
# macOS resource forks
._*
```

3. **Commit the cleanup:**
```bash
git add .github/workflows/._*
git commit -m "ci: remove macOS resource fork files from workflows"
```

4. **Optional: Configure macOS to not create resource forks on network volumes:**
```bash
# Add to developer's ~/.bashrc or similar:
defaults write com.apple.desktopservices DSDontWriteNetworkStores true
```

## References
- [macOS resource forks](https://en.wikipedia.org/wiki/Resource_fork)
- [AppleDouble format](https://en.wikipedia.org/wiki/AppleDouble)
- [Git ignore macOS files](https://github.com/github/gitignore/blob/main/global/macOS.gitignore)
