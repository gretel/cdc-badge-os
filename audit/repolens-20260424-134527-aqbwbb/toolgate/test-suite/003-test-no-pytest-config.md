---
title: "[LOW] Python tests lack pytest configuration"
severity: LOW
domain: test-suite
lens: test-suite
labels:
  - "audit:toolgate/test-suite"
  - "python"
  - "configuration"
---

## Summary
The Python test suite in `third_party/libtropic/vendor/trezor_crypto/tests/` lacks a pytest configuration file. This makes test discovery and execution less convenient and doesn't capture common test options.

**Files affected:**
- `third_party/libtropic/vendor/trezor_crypto/tests/test_bignum.py`
- `third_party/libtropic/vendor/trezor_crypto/tests/test_curves.py`
- `third_party/libtropic/vendor/trezor_crypto/tests/test_wycheproof.py`

## Impact
- **Developer friction**: Need to specify options on command line each time
- **Inconsistent test runs**: Different developers may use different options
- **Missing best practices**: No configured markers, timeout, or parallel execution settings

## Evidence
No pytest configuration files exist in the test directory:
```
$ find third_party/libtropic/vendor/trezor_crypto/tests -name "pytest.ini" -o -name "conftest.py"
(no results)
```

Tests use pytest features (fixtures, parametrize) that would benefit from configuration:
- `test_bignum.py` uses `@pytest.fixture(params=...)` for parameterized tests
- `test_wycheproof.py` uses pytest for structured test data from JSON files

## Recommended Fix
Add a `pytest.ini` or `pyproject.toml` configuration file in `third_party/libtropic/vendor/trezor_crypto/tests/`:

```ini
# pytest.ini
[pytest]
testpaths = .
python_files = test_*.py
python_functions = test_*
markers =
    slow: marks tests as slow (deselect with '-m "not slow"')
    requires_lib: tests requiring libtrezor-crypto.so
addopts = -v --tb=short
```

Or in `pyproject.toml`:
```toml
[tool.pytest.ini_options]
testpaths = ["tests"]
python_files = "test_*.py"
python_functions = "test_*"
addopts = "-v --tb=short"
```

## References
- [pytest configuration docs](https://docs.pytest.org/en/latest/reference/customize.html)
- [test_bignum.py](third_party/libtropic/vendor/trezor_crypto/tests/test_bignum.py) - Uses fixtures and parametrize
