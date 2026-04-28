---
title: "[MEDIUM] Python dependencies not pinned with lock file"
severity: MEDIUM
domain: infra-reproducibility
lens: devops
labels:
  - "audit:devops/infra-reproducibility"
  - "dependencies"
---

## Summary
The Python dependencies for the flash tool (`tools/requirements.txt`) are not pinned to specific versions, and there is no `requirements.txt` with hashes or a `.lock` file. The GitHub Actions workflow uses `pip install platformio` without version pinning.

**Files affected:**
- `tools/requirements.txt` - Unpinned versions (`esptool>=4.7`, `requests>=2.28`, `bleak>=0.21`, `esp-coredump>=1.5`)
- `.github/workflows/build.yml:31-32` - `pip install platformio` without version
- `.github/workflows/deploy-pages.yml` - Doxygen installation from arbitrary URLs

## Impact
- **Environment drift**: Different developers or CI runs may get different versions of dependencies, potentially causing build failures or different behavior
- **Security**: Unpinned dependencies can silently pull in vulnerable versions
- **Reproducibility**: Builds may not be deterministic across time
- **CI reliability**: `platformio` without version pinning can break when new major versions are released

## Evidence
`tools/requirements.txt`:
```
# flash_firmware.py
esptool>=4.7
requests>=2.28

# ble_serial.py
bleak>=0.21

# coredump.py
esp-coredump>=1.5
```

`.github/workflows/build.yml:31-32`:
```yaml
- name: Install PlatformIO
  run: |
    python -m pip install --upgrade pip
    pip install platformio
```

## Recommended Fix
1. **Pin Python dependencies** in `tools/requirements.txt`:
   ```bash
   # Generate hashes and pinned versions
   pip freeze > tools/requirements.txt
   # Or use pip-compile from pip-tools for better control
   ```

2. **Pin PlatformIO version** in workflow:
   ```yaml
   pip install platformio==9.0.2  # Use specific version
   ```

3. **Add a `requirements.lock` file** or use `pip-tools` workflow:
   - Create `tools/requirements.in` with unpinned specs
   - Generate `tools/requirements.txt` with `pip-compile`
   - Commit the locked file

4. **Use `pip install -r requirements.txt`** instead of `pip install platformio` in CI

## References
- [Python packaging best practices](https://packaging.python.org/en/latest/guides/distributing-packages-using-setuptools/#install-requirements)
- [pip-tools for dependency management](https://github.com/jazzband/pip-tools)
- [GitHub Actions Python CI template](https://github.com/actions/starter-workflows/blob/main/ci/python-package.yml)
