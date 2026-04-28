---
title: "[LOW] Python requirements not pinned to specific versions"
severity: LOW
domain: linting
lens: code-quality/linting
labels:
  - "audit:code-quality/linting"
---

## Summary
The Python requirements file uses only minimum version constraints (`>=`) instead of pinned versions (`==`). This can lead to unexpected behavior when newer versions of dependencies are installed.

**Location:** `tools/requirements.txt`

## Impact
- **Reproducibility issues**: Different developers or CI may get different dependency versions
- **Breaking changes**: New major versions could introduce breaking changes
- **CI drift**: Builds may start failing when upstream packages update

## Evidence

**File:** `tools/requirements.txt`
```
# flash_firmware.py
esptool>=4.7
requests>=2.28

# ble_serial.py
bleak>=0.21

# coredump.py
esp-coredump>=1.5
```

All dependencies use `>=` (minimum version) instead of `==` (pinned version).

## Recommended Fix

Pin dependencies to specific versions:

```
# flash_firmware.py
esptool==4.7.0
requests==2.31.0

# ble_serial.py
bleak==0.21.0

# coredump.py
esp-coredump==1.5.0
```

### Process:
1. Check current versions being used:
   ```bash
   pip list | grep -E "esptool|requests|bleak|esp-coredump"
   ```

2. Update `requirements.txt` with pinned versions

3. Consider adding a `requirements-dev.txt` for development-only dependencies

4. Add a CI step to verify dependencies install correctly

### Alternative: Use compatible release operator
If you want to allow minor updates:
```
esptool~=4.7.0  # Allows 4.7.x but not 4.8.0
requests~=2.28.0
```

## References
- [Python packaging - Specifying requirements](https://packaging.python.org/en/latest/tutorials/installing-packages/#specifying-versions)
- [PIP requirements best practices](https://pip.pypa.io/en/stable/reference/requirements-file-format/)
