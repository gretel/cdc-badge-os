---
title: "[LOW] Python Dependencies Use Loose Version Pinning"
severity: LOW
domain: dependency-management
lens: maintainability
labels:
  - "python"
  - "dependencies"
  - "ci-cd"
---

## Summary

Python dependencies in `tools/requirements.txt` use loose version constraints (`>=`) instead of exact pinning. This can lead to build inconsistencies when different developers or CI environments install different versions.

**Current `tools/requirements.txt`:**
```python
# flash_firmware.py
esptool>=4.7
requests>=2.28

# ble_serial.py
bleak>=0.21

# coredump.py
esp-coredump>=1.5
```

**Third-party requirements files:**
- `third_party/libtropic/scripts/test_runner/requirements.txt`: Uses exact pinning (`pyserial==3.5`, `telnetlib3==2.0.4`)
- `third_party/libtropic/docs/requirements.txt`: No version constraints at all

**Inconsistency:**
- Main project: `>=` (minimum version)
- Test runner: `==` (exact version)
- Docs: No version specified

## Impact

1. **Build Reproducibility**: Different versions may be installed on different machines
2. **Breaking Changes**: New major versions could break scripts (e.g., esptool 5.0 might change CLI)
3. **Debug Difficulty**: Hard to reproduce issues when versions vary
4. **CI/CD Flakiness**: Tests may pass on one environment and fail on another

## Recommended Fix

**Update `tools/requirements.txt` with exact pinning:**

```python
# flash_firmware.py
esptool==4.7.2
requests==2.31.0

# ble_serial.py
bleak==0.21.1

# coredump.py
esp-coredump==1.5.0
```

**Or use a lock file (recommended):**

Generate `requirements.txt` with `pip-tools`:
```bash
pip install pip-tools
pip-compile tools/requirements.in  # Create .in file with loose constraints
pip-sync tools/requirements.txt    # Install exact versions
```

**Add version check to CI:**

```yaml
- name: Install and verify dependencies
  run: |
    pip install -r tools/requirements.txt
    pip freeze > requirements.freeze.txt
    cat requirements.freeze.txt
```

**Optional: Add a `requirements-dev.txt`** for development tools:
```python
-r requirements.txt

# Development tools
pytest==7.4.0
black==23.12.0
mypy==1.8.0
```

## References

- [pip-compile documentation](https://pip-tools.readthedocs.io/)
- [Python packaging best practices](https://packaging.python.org/en/latest/guides/installing-using-pip-and-requirements-files/)
- [Semantic Versioning](https://semver.org/)
