---
title: "[MEDIUM] Git submodules not pinned to specific commits"
severity: MEDIUM
domain: devops
lens: dependency-management
labels:
  - "audit:devops/dependency-management"
---

## Summary
Git submodules (CalEPD, libtropic, Adafruit-GFX) are defined in `.gitmodules` but don't specify pinned commit SHAs or specific branches. This can lead to submodule HEAD moving and introducing unexpected changes during builds.

**File:** `.gitmodules`

## Impact
- **Unpredictable updates**: Submodule `git pull` can move to different commits
- **Build breaks**: Upstream repository changes may break the build silently
- **Hard to debug**: Different developers may have different submodule states when reporting issues
- **Release inconsistency**: Tagged releases may point to different submodule states

## Evidence
Current `.gitmodules` (lines 1-11):
```ini
[submodule "components/CalEPD"]
    path = components/CalEPD
    url = https://github.com/martinberlin/CalEPD.git
    ignore = dirty

[submodule "third_party/libtropic"]
    path = third_party/libtropic
    url = https://github.com/tropicsquare/libtropic.git
    ignore = dirty

[submodule "components/Adafruit-GFX"]
    path = components/Adafruit-GFX
    url = https://github.com/martinberlin/Adafruit-GFX-Library-ESP-IDF
```

Missing:
- No `branch` specification for any submodule
- No pinned commit SHAs

Note: The `ignore = dirty` setting helps detect uncommitted changes but doesn't solve the core issue of unpinned HEAD.

File: `tools/pio_submodules.py` (lines 1-30)
- Uses `git submodule update --init --recursive` without specific revision
- Only checks if directory is "populated", not if correct version

## Recommended Fix

**Option 1: Pin to specific commits** (recommended for stability)

```bash
# Navigate to each submodule and find the desired commit
cd components/CalEPD
git log --oneline -10  # Review recent commits
git checkout <commit-sha>

# Update the parent repo
cd ..
git add components/CalEPD
git commit -m "Pin CalEPD submodule to v1.2.0 (abc123)"
```

Repeat for each submodule:
- `components/CalEPD` - Pin to a release tag if available
- `third_party/libtropic` - Pin to a release tag
- `components/Adafruit-GFX` - Pin to a specific commit

**Option 2: Add branch specification**

Update `.gitmodules`:
```ini
[submodule "components/CalEPD"]
    path = components/CalEPD
    url = https://github.com/martinberlin/CalEPD.git
    branch = v1.2.0
    ignore = dirty

[submodule "third_party/libtropic"]
    path = third_party/libtropic
    url = https://github.com/tropicsquare/libtropic.git
    branch = v1.15.0
    ignore = dirty
```

**Option 3: Add CI check for submodule drift**

Add to `.github/workflows/build.yml`:
```yaml
- name: Verify submodules are pinned
  run: |
    # Check each submodule is on expected branch/tag
    git submodule foreach 'git describe --exact-match --tags || echo "Warning: Not on tagged version"'
```

**Documentation update:**

Add a `SUBMODULES.md` file or section in README:
```
## Submodule Versions

| Submodule | Version | Commit |
|-----------|---------|--------|
| CalEPD | v1.2.0 | abc1234... |
| libtropic | v1.15.0 | def5678... |
| Adafruit-GFX | main | ghi9012... |

### Updating Submodules

```bash
# Update all submodules to latest on their branches
git submodule update --remote --merge

# Update specific submodule
git submodule update --remote components/CalEPD
```
```

## References
- [Git Submodules Documentation](https://git-scm.com/book/en/v2/Git-Tools-Submodules)
- [Pinning Submodules Best Practices](https://stackoverflow.com/questions/17628036/pinning-a-submodule-to-a-specific-commit)
- [Git submodule set-branch](https://git-scm.com/docs/git-submodule#Documentation/git-submodule.txt-set-branch)

---
**Related issues:** None
**Estimated effort:** 1 hour
