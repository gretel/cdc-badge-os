---
title: "[LOW] Python dependencies lack lock file for reproducible builds"
severity: LOW
domain: sbom-supply-chain
lens: compliance
labels:
  - "dependencies:python"
  - "reproducibility"
---

## Summary
The `tools/requirements.txt` file has no corresponding lock file (e.g., `requirements.lock.txt` or `poetry.lock`). This means Python dependencies are not pinned to exact versions, which can lead to non-reproducible builds and unexpected behavior when new versions are released.

**Location:** `tools/requirements.txt` - No lock file present.

## Impact
- **Non-reproducible builds:** Different developers or CI runs may get different dependency versions
- **Security Risk:** New versions of dependencies may introduce breaking changes or vulnerabilities
- **Version Drift:** Tools like `esptool`, `bleak`, `requests` may upgrade unexpectedly

## Evidence
Current `tools/requirements.txt`:
```txt
# flash_firmware.py
esptool>=4.7
requests>=2.28

# ble_serial.py
bleak>=0.21

# coredump.py
esp-coredump>=1.5
```

- Uses `>=` version specifiers (not exact pins)
- No `requirements.lock.txt` or `poetry.lock` in repository
- No lock file generation in CI/CD

## Recommended Fix
1. **Generate lock file** using pip-tools or Poetry:
```bash
# Option 1: pip-tools
pip install pip-tools
pip-compile tools/requirements.txt --output-file tools/requirements.lock.txt

# Option 2: Poetry
poetry init
poetry add -f tools/requirements.txt
poetry lock
```

2. **Add lock file to CI** for validation:
```yaml
- name: Install dependencies
  run: pip install -r tools/requirements.lock.txt
```

3. **Update dependency workflow:**
```yaml
- name: Update dependencies
  run: |
    pip-compile tools/requirements.txt --output-file tools/requirements.lock.txt
    git add tools/requirements.lock.txt
```

## References
- [pip-tools](https://github.com/jazzband/pip-tools)
- [Python Packaging User Guide - Lock Files](https://packaging.python.org/en/latest/guides/creating-a-lock-file/)
- [Reproducible Builds](https://reproducible-builds.org/)
