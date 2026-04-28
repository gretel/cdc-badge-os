---
title: "[SETUP] Add type checking integration for Python utility scripts"
severity: MEDIUM
domain: build
lens: toolgate/typecheck
labels:
  - "audit:toolgate/typecheck"
  - "setup"
---

## Summary
The repository contains Python utility scripts in `tools/` but has no type checker configuration or CI integration. No type checkers (mypy, pyright) are configured in `pyproject.toml`, `mypy.ini`, or `setup.cfg`, and no CI workflow includes a type checking step.

**Files affected:**
- `tools/ble_serial.py`
- `tools/coredump.py`
- `tools/flash_firmware.py`
- `tools/pio_component_manager.py`
- `tools/pio_submodules.py`

## Impact
Without type checking:
- Type-related bugs in utility scripts may go undetected
- No runtime type safety for tooling that developers use to flash firmware and analyze core dumps
- Missing documentation of expected types in function signatures

## Evidence
**No type checker configuration found:**
- No `pyproject.toml` with `[tool.mypy]` or `[tool.pyright]` section
- No `mypy.ini` or `setup.cfg` with `[mypy]` section
- No CI workflow step for type checking in `.github/workflows/build.yml`

**Python scripts use type hints but no validation:**
- `tools/ble_serial.py` uses modern type hints (e.g., `str | None`, `list`, `bytearray`)
- `tools/pio_submodules.py` uses type hints (e.g., `Path`, `list[str]`)
- These hints are not validated by any tool

## Recommended Fix
1. Create `pyproject.toml` in project root with mypy configuration:
   ```toml
   [tool.mypy]
   files = "tools/"
   python_version = "3.11"
   warn_return_any = true
   warn_unused_configs = true
   disallow_untyped_defs = false  # Allow gradual typing
   ```

2. Add type checking step to `.github/workflows/build.yml`:
   ```yaml
   - name: Install mypy
     run: pip install mypy

   - name: Type check
     run: mypy tools/
   ```

3. (Optional) Install dependencies for type checking:
   ```bash
   pip install mypy bleak types-requests
   ```

## References
- [mypy documentation](https://mypy.readthedocs.io/)
- [Python typing documentation](https://docs.python.org/3/library/typing.html)
- [GitHub Actions Python workflow](https://docs.github.com/en/actions/automating-builds-and-tests/building-and-testing-python)
