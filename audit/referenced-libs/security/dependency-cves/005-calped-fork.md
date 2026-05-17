---
title: "[LOW] CalEPD Submodule Points to Fork Instead of Upstream"
severity: LOW
domain: dependencies
lens: dependency-cves
labels:
  - "audit:security/dependency-cves"
---

## Summary
The CalEPD component is cloned from a fork (`https://github.com/martinberlin/CalEPD.git`) instead of the original upstream repository. This may lead to missing upstream bug fixes, security patches, and feature updates.

**Files affected:**
- `.gitmodules` (line 3: `url = https://github.com/martinberlin/CalEPD.git`)
- `components/CalEPD/` - Contains forked version of CalEPD

## Impact
1. **Missing upstream patches**: Security fixes and bug fixes from the main repository may not be included.
2. **Divergence risk**: The fork may diverge significantly from upstream over time.
3. **Maintenance burden**: Need to manually track and merge upstream changes.

## Evidence
From `.gitmodules`:
```ini
[submodule "components/CalEPD"]
    path = components/CalEPD
    url = https://github.com/martinberlin/CalEPD.git
```

The original (upstream) CalEPD repository is at: https://github.com/pepiburr/CalEPD (or similar)

Commit used: `ba865e87d3d1da9ed54316f6bf28601d1ffd2caf` (tag: 1.1-18-gba865e8)

## Recommended Fix
1. **Evaluate the fork**: Determine why the fork is used (specific changes needed?).

2. **If fork changes are minimal**, consider switching to upstream:
   ```bash
   git submodule set-url components/CalEPD https://github.com/pepiburr/CalEPD.git
   git submodule update --remote components/CalEPD
   ```

3. **If fork changes are needed**, document them and create a plan to upstream the changes.

4. **Regularly sync from upstream** to get security patches:
   ```bash
   cd components/CalEPD
   git fetch upstream
   git merge upstream/main
   cd ..
   git add components/CalEPD
   git commit -m "Sync CalEPD with upstream"
   ```

## References
- CalEPD GitHub: https://github.com/martinberlin/CalEPD
- ESP-IDF component registry: https://components.espressif.com/
