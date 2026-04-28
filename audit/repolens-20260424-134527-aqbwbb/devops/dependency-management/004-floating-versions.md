---
title: "[LOW] Python dependencies use floating version specifiers"
severity: LOW
domain: devops
lens: dependency-management
labels:
  - "audit:devops/dependency-management"
---

## Summary
Python dependencies in `tools/requirements.txt` use floating version specifiers (`>=`) instead of pinned versions. This allows pip to install any future version, potentially introducing breaking changes or bugs.

**File:** `tools/requirements.txt`

## Impact
- **Build inconsistency**: Different versions may be installed on different machines or CI runs
- **Breaking changes**: `>=` allows major version upgrades that may have breaking API changes
- **Debug difficulty**: Hard to reproduce issues when versions aren't fixed
- **Minor impact**: These are development/tools dependencies, not runtime firmware dependencies

## Evidence
Current `tools/requirements.txt`:
```
# flash_firmware.py
esptool>=4.7
requests>=2.28

# ble_serial.py
bleak>=0.21

# coredump.py
esp-coredump>=1.5
```

All packages use `>=` which allows:
- `esptool>=4.7` → could install 4.7, 5.0, 6.0, etc.
- `requests>=2.28` → could install 2.28, 3.0, etc.
- `bleak>=0.21` → could install 0.21, 1.0, etc.
- `esp-coredump>=1.5` → could install 1.5, 2.0, etc.

## Recommended Fix
Pin dependencies to specific versions for deterministic builds:

```
# flash_firmware.py
esptool==4.7.0
requests==2.31.0

# ble_serial.py
bleak==0.21.0

# coredump.py
esp-coredump==1.5.0
```

Or use compatible release specifiers for more flexibility:
```
esptool~=4.7
requests~=2.28
bleak~=0.21
esp-coredump~=1.5
```

Then generate a lock file using `pip-tools`:
```bash
pip install pip-tools
pip-compile tools/requirements.txt  # Creates requirements.txt with pinned versions
```

## References
- [Python Packaging User Guide - Specifying Dependencies](https://packaging.python.org/en/latest/tutorials/installing-packages/#installing-with-versions)
- [pip-tools documentation](https://pip-tools.readthedocs.io/)
- [Semantic Versioning](https://semver.org/)

---
**Related issues:** None
**Estimated effort:** 30 minutes
