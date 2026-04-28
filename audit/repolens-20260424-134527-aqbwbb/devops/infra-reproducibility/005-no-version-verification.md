---
title: "[LOW] No build environment version verification in CI"
severity: LOW
domain: infra-reproducibility
lens: devops
labels:
  - "audit:devops/infra-reproducibility"
  - "ci-cd"
---

## Summary
The CI workflow does not verify or report the versions of key build tools (PlatformIO, ESP-IDF, Python) after installation. This makes it harder to debug build failures and track when builds might be affected by tool updates.

**Files affected:**
- `.github/workflows/build.yml` - Build workflow without version logging

## Impact
- **Debugging difficulty**: When builds fail due to tool version changes, there's no easy way to see what versions were used
- **Audit trail**: Harder to reproduce historical build environments
- **Silent breaking changes**: Tool updates might change behavior without clear indication

## Evidence
`.github/workflows/build.yml` - No version verification steps:
```yaml
- name: Install PlatformIO
  run: |
    python -m pip install --upgrade pip
    pip install platformio

- name: Build firmware
  run: pio run
```

Missing:
- `pio --version`
- `python --version`
- ESP-IDF version verification

## Recommended Fix
Add version verification steps to the build workflow:

```yaml
- name: Install PlatformIO
  run: |
    python -m pip install --upgrade pip
    pip install platformio

- name: Verify build environment
  run: |
    echo "=== Build Environment ==="
    python --version
    pio --version
    pio pkg list
```

This provides clear output of what versions are being used for each build.

## References
- [PlatformIO CLI documentation](https://docs.platformio.org/en/latest/core.html)
