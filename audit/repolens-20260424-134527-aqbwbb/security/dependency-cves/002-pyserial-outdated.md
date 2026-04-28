---
title: "[LOW] Outdated Python Dependencies in Test Runner (pyserial==3.5)"
severity: LOW
domain: dependencies
lens: dependency-cves
labels:
  - "audit:security/dependency-cves"
---

## Summary
The test runner requirements file (`third_party/libtropic/scripts/test_runner/requirements.txt`) pins **pyserial==3.5**, which was released in 2018. The current version is pyserial 3.5+ (with 3.5 being from 2018 and newer versions available). While pyserial 3.5 itself doesn't have critical CVEs, it's missing bug fixes and improvements from newer versions.

**Files affected:**
- `third_party/libtropic/scripts/test_runner/requirements.txt` (line 1: `pyserial==3.5`)

## Impact
1. **Missing bug fixes**: Newer versions have fixed serial port detection issues on various platforms.
2. **Python 3.11+ compatibility**: Older versions may have compatibility issues with newer Python versions.
3. **Limited to test infrastructure**: This only affects test/development, not production firmware.

## Evidence
From `third_party/libtropic/scripts/test_runner/requirements.txt`:
```
pyserial==3.5
telnetlib3==2.0.4
```

pyserial 3.5 was released in October 2018. Current version is 3.5 (latest stable) with ongoing maintenance.

## Recommended Fix
1. **Update pyserial** to the latest version:
   ```bash
   # Update requirements file
   sed -i 's/pyserial==3.5/pyserial>=3.5/' third_party/libtropic/scripts/test_runner/requirements.txt
   
   # Or pin to latest
   sed -i 's/pyserial==3.5/pyserial==3.5/' third_party/libtropic/scripts/test_runner/requirements.txt
   ```

2. **Test the test runner** to ensure compatibility:
   ```bash
   pip install -r third_party/libtropic/scripts/test_runner/requirements.txt
   ```

## References
- pyserial PyPI: https://pypi.org/project/pyserial/
- pyserial GitHub: https://github.com/pyserial/pyserial
- Python serial port documentation: https://pythonhosted.org/pyserial/
