---
title: "[MEDIUM] Git submodules not pinned to specific commits"
severity: MEDIUM
domain: infra-reproducibility
lens: devops
labels:
  - "audit:devops/infra-reproducibility"
  - "dependencies"
---

## Summary
Git submodules are defined but may not be pinned to specific commits in the CI workflow. The `build.yml` uses `submodules: recursive` but this fetches whatever the `.gitmodules` points to, which could be a branch tip that changes over time.

**Files affected:**
- `.gitmodules` - Submodule definitions
- `.github/workflows/build.yml:15-17` - Checkout with recursive submodules
- `tools/pio_submodules.py` - Custom submodule initialization script

## Impact
- **Build drift**: Submodules can silently update when the target branch moves, causing unexpected build changes
- **Reproducibility**: Historical builds may not be reproducible if submodules moved
- **CI instability**: A submodule update could break the build without any change to the main repository

## Evidence
`.gitmodules`:
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

`.github/workflows/build.yml:15-17`:
```yaml
- name: Checkout repository
  uses: actions/checkout@v4
  with:
    submodules: recursive
```

Note: `ignore = dirty` in `.gitmodules` means local changes won't trigger CI failures.

## Recommended Fix
1. **Ensure submodules are pinned to specific commits** (this is done automatically when you commit submodule updates):
   ```bash
   git submodule update --init --recursive
   git add components/CalEPD components/Adafruit-GFX third_party/libtropic
   git commit -m "Pin submodules to specific commits"
   ```

2. **Add a CI check to verify submodules are pinned**:
   ```yaml
   - name: Check submodules are pinned
     run: |
       git submodule foreach --quiet 'git describe --exact-match --tags HEAD || git log -1 --format="%h"'
   ```

3. **Consider removing `ignore = dirty`** to catch local changes in CI:
   ```ini
   [submodule "components/CalEPD"]
       path = components/CalEPD
       url = https://github.com/martinberlin/CalEPD.git
   ```

## References
- [Git submodules best practices](https://git-scm.com/book/en/v2/Git-Tools-Submodules)
- [GitHub Actions checkout action](https://github.com/actions/checkout)
