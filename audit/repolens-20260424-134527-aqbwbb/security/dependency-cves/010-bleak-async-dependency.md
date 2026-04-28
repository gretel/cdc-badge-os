---
title: "[LOW] bleak Dependency for BLE Serial - Async I/O Version Compatibility"
severity: LOW
domain: dependencies
lens: dependency-cves
labels:
  - "audit:security/dependency-cves"
---

## Summary
The `tools/requirements.txt` file includes **bleak>=0.21** for BLE serial communication. While bleak is a well-maintained library, async I/O libraries can have subtle version compatibility issues with Python versions and underlying BlueZ (Linux) or CoreBluetooth (macOS) backends.

**Files affected:**
- `tools/requirements.txt` (line 6: `bleak>=0.21`)
- `tools/ble_serial.py` - Uses bleak for BLE communication

## Impact
1. **Async compatibility**: Different Python versions (3.10, 3.11, 3.12) have different asyncio semantics that may affect bleak behavior.
2. **Backend differences**: Linux (BlueZ), macOS (CoreBluetooth), and Windows (WinRT) backends may have different bug profiles.
3. **No lock file**: As noted in finding #3, the lack of a lock file means different versions may be installed across environments.

## Evidence
From `tools/requirements.txt`:
```
# ble_serial.py
bleak>=0.21
```

From `tools/ble_serial.py` (likely usage):
```python
from bleak import BleakClient, BleakScanner
# BLE operations...
```

bleak v0.21 was released in 2022. Current version is 0.21.x with ongoing maintenance.

## Recommended Fix
1. **Pin to a specific version** in `tools/requirements.txt`:
   ```
   bleak==0.21.1  # or latest stable
   ```

2. **Create a lock file** using pip-tools:
   ```bash
   pip install pip-tools
   cd tools
   pip-compile requirements.txt
   ```

3. **Test on all platforms** where the BLE serial tool is used (Linux, macOS, Windows).

4. **Add version check** to the script:
   ```python
   import bleak
   assert tuple(map(int, bleak.__version__.split('.')[:2])) >= (0, 21)
   ```

## References
- bleak PyPI: https://pypi.org/project/bleak/
- bleak GitHub: https://github.com/hbldh/bleak
- bleak documentation: https://bleak.readthedocs.io/
- Python asyncio: https://docs.python.org/3/library/asyncio.html
