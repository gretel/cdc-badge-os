---
title: "[LOW] Missing Lock File for Python Dependencies in Tools"
severity: LOW
domain: dependencies
lens: dependency-cves
labels:
  - "audit:security/dependency-cves"
---

## Summary
The `tools/requirements.txt` file uses version ranges (e.g., `esptool>=4.7`, `requests>=2.28`, `bleak>=0.21`) but there is **no corresponding lock file** (like `requirements.txt` with pinned versions or `Pipfile.lock`). This leads to non-deterministic builds where different versions of dependencies may be installed over time.

**Files affected:**
- `tools/requirements.txt` - Uses `>=` version constraints without lock file

## Impact
1. **Non-deterministic builds**: Different developers or CI systems may install different versions of dependencies.
2. **Dependency confusion risk**: Newer versions pulled in may have breaking changes or vulnerabilities.
3. **Harder to audit**: Security audits require knowing exact dependency versions.
4. **Potential for regressions**: A new minor version of a dependency could break the tooling.

## Evidence
From `tools/requirements.txt`:
```
# flash_firmware.py
esptool>=4.7
requests>=2.28

# ble_serial.py
bleak>=0.21

# coredump.py
esp-coredump>=1.5
```

Note: The `dependencies.lock` file exists for ESP-IDF components, but there's no lock file for Python tool dependencies.

## Recommended Fix
1. **Create a lock file** for Python dependencies:
   ```bash
   cd tools
   pip install pip-tools  # or use pip freeze
   pip-compile requirements.txt  # Creates requirements.txt with pinned versions
   ```

2. **Alternative - Pin exact versions**:
   ```bash
   # Update tools/requirements.txt with exact versions
   esptool==4.7.0
   requests==2.31.0
   bleak==0.21.1
   esp-coredump==1.5
   ```

3. **Commit the lock file** to version control.

4. **Update CI/build scripts** to use the locked versions.

## References
- Python packaging best practices: https://packaging.python.org/en/latest/guides/installing-using-pip-and-requirements-files/
- pip-tools documentation: https://pip-tools.readthedocs.io/
- Dependency management for Python: https://pythonguides.com/python-dependency-management/
