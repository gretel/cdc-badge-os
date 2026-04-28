---
title: "[MEDIUM] Python test suite dependencies missing (pytest, shared library)"
severity: MEDIUM
domain: test-suite
lens: test-suite
labels:
  - "audit:toolgate/test-suite"
  - "setup"
  - "python"
---

## Summary
The Python test suite in `third_party/libtropic/vendor/trezor_crypto/tests/` cannot run due to missing dependencies:

1. **pytest** is not installed in the environment
2. **libtrezor-crypto.so** shared library is not built (required by all Python tests via ctypes)
3. Python packages `pyasn1`, `ecdsa`, `curve25519` are not installed (required by `test_curves.py` and `test_wycheproof.py`)

**Files affected:**
- `third_party/libtropic/vendor/trezor_crypto/tests/test_bignum.py` (line 18: `import pytest`, line 20: loads `libtrezor-crypto.so`)
- `third_party/libtropic/vendor/trezor_crypto/tests/test_curves.py` (line 7-8: imports `curve25519`, `ecdsa`)
- `third_party/libtropic/vendor/trezor_crypto/tests/test_wycheproof.py` (line 8: `import pytest`, line 9: `from pyasn1...`)
- `third_party/libtropic/vendor/trezor_crypto/Makefile` (target `tests/libtrezor-crypto.so`)

## Impact
- **Test coverage gap**: All Python-based unit tests for the TROPIC01 secure element's cryptographic functions (bignum, curves, ECDSA, EdDSA) cannot be executed
- **Regression risk**: Changes to cryptography code in `third_party/libtropic/vendor/trezor_crypto/` cannot be validated without running these tests
- **CI/CD block**: Automated test pipelines will fail or skip these tests silently

## Evidence
Running test collection shows pytest is missing:
```
$ cd third_party/libtropic/vendor/trezor_crypto/tests
$ python3 -m pytest --collect-only
/usr/bin/python3: No module named pytest
```

Shared library not present:
```
$ ls -la tests/libtrezor-crypto.so
ls: cannot access 'tests/libtrezor-crypto.so': No file or directory
```

Test file dependencies (from `test_bignum.py` line 18-21):
```python
import pytest

dir = os.path.abspath(os.path.dirname(__file__))
lib = ctypes.cdll.LoadLibrary(os.path.join(dir, "libtrezor-crypto.so"))
```

## Recommended Fix
1. **Build the shared library**:
   ```bash
   cd third_party/libtropic/vendor/trezor_crypto
   make tests/libtrezor-crypto.so
   ```

2. **Install Python dependencies**:
   ```bash
   pip install pytest pyasn1 ecdsa curve25519
   ```
   Or use the project's requirements file if available:
   ```bash
   pip install -r third_party/libtropic/scripts/test_runner/requirements.txt
   ```

3. **Verify tests can run**:
   ```bash
   cd third_party/libtropic/vendor/trezor_crypto/tests
   python3 -m pytest test_bignum.py -v --tb=short
   ```

## References
- [Trezor-Crypto Makefile](third_party/libtropic/vendor/trezor_crypto/Makefile) - Shows `tests:` target and library build rules
- [test_bignum.py](third_party/libtropic/vendor/trezor_crypto/tests/test_bignum.py:18-21) - Shows library loading via ctypes
- [Python test files](third_party/libtropic/vendor/trezor_crypto/tests/) - Directory containing pytest-based tests
