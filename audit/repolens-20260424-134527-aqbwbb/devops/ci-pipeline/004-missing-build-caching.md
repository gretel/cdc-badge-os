---
title: "[MEDIUM] Missing build caching causing slow CI pipeline"
severity: MEDIUM
domain: devops
lens: ci-pipeline
labels:
  - "audit:devops/ci-pipeline"
---

## Summary
The CI pipeline does not use any caching for dependencies or build artifacts. Each build starts fresh, installing PlatformIO and all dependencies from scratch. This increases build time significantly and wastes CI resources.

**Evidence:**
- File: `.github/workflows/build.yml` - Lines 18-33
- No `actions/cache` for Python packages, PlatformIO, or ESP-IDF dependencies
- `pip install platformio` runs on every build without cache

## Impact
- Each build takes longer than necessary (PlatformIO setup alone can take 2-3 minutes)
- Increased CI costs and slower feedback for developers
- More load on PyPI and PlatformIO registries
- No incremental builds - full rebuild every time

## Evidence
```yaml
- name: Setup Python
  uses: actions/setup-python@v5
  with:
    python-version: '3.11'

- name: Install PlatformIO
  run: |
    python -m pip install --upgrade pip
    pip install platformio
# No cache step before this
```

## Recommended Fix
Add caching for Python dependencies and PlatformIO:

```yaml
- name: Cache PlatformIO packages
  uses: actions/cache@v4
  with:
    path: |
      ~/.cache/pip
      ~/.platformio
    key: ${{ runner.os }}-platformio-${{ hashFiles('**/requirements.txt', '**/platformio.ini') }}

- name: Install PlatformIO
  run: |
    python -m pip install --upgrade pip
    pip install platformio
```

Also consider caching ESP-IDF if used directly (though PlatformIO manages this internally).

## References
- [actions/cache](https://github.com/actions/cache)
- [PlatformIO CI/CD best practices](https://docs.platformio.org/en/latest/integrations/ci/github-actions.html)
