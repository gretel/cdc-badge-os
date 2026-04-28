---
title: "[LOW] CalEPD submodule at v1.1-18-gba865e8, 52 commits behind master"
severity: LOW
domain: dependencies
lens: upgrade-paths
labels:
  - "audit:maintainability/upgrade-paths"
---

## Summary
The `components/CalEPD` submodule is at tag **v1.1-18-gba865e8** (commit `ba865e8`), which is 52 commits behind the master branch. The latest release is **v1.1** (same version tag, but master has additional fixes).

**Evidence:**
- Current: `components/CalEPD (1.1-18-gba865e8)` - commit `ba865e87d3d1da9ed54316f6bf28601d1ffd2caf`
- Latest tag: v1.1 (same version, but master has 52 more commits)
- File: `.gitmodules` and git submodule status

## Impact
**Potential improvements in master:**
- Added support for GDEQ037T31 and GDEM029E97 display models
- Bug fixes and display model additions since v1.1 tag
- Better touch model support

**Risk:**
- Minimal - CalEPD is a stable display driver library
- Project has custom patch `patches/calepd_spi_miso.patch` that may need review

## Evidence
From `git submodule status`:
```
ba865e87d3d1da9ed54316f6bf28601d1ffd2caf components/CalEPD (1.1-18-gba865e8)
```

Current submodule in `.gitmodules`:
```
[submodule "components/CalEPD"]
    path = components/CalEPD
    url = https://github.com/martinberlin/CalEPD.git
```

Master is 52 commits ahead of v1.1 tag.

## Recommended Fix
1. **Review commit history** to see what changed:
   ```bash
   cd components/CalEPD
   git log --oneline v1.1..master | head -20
   ```

2. **Update if needed**:
   ```bash
   git fetch --tags
   git checkout master  # or specific commit for latest v1.1.x
   cd ../..
   git add components/CalEPD
   git commit -m "chore: update CalEPD to latest v1.1.x"
   ```

3. **Test patch**: Verify `patches/calepd_spi_miso.patch` still applies cleanly.

4. **Build test**: Verify E-Paper display still works correctly.

## References
- [CalEPD releases](https://github.com/martinberlin/CalEPD/releases)
- [CalEPD master branch](https://github.com/martinberlin/CalEPD/commits/master)
