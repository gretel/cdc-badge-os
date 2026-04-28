---
title: "[MEDIUM] Missing Python type checker configuration for tools"
severity: MEDIUM
domain: python
lens: toolgate/typecheck
labels:
  - audit:toolgate/typecheck
---

## Summary
The CDC Badge OS repository contains Python utility scripts in the `tools/` directory but has no configured type checker (mypy or pyright). The project has 5 Python files that would benefit from type checking:
- `tools/ble_serial.py` (196 lines)
- `tools/flash_firmware.py` (262 lines)
- `tools/coredump.py` (134 lines)
- `tools/pio_component_manager.py` (4 lines)
- `tools/pio_submodules.py` (41 lines)

Only 2 functions across all files have type annotations (both in `pio_submodules.py`).

## Impact
Without type checking:
- Type errors in utility scripts may go undetected until runtime
- No CI validation of Python code quality
- Reduced maintainability as scripts grow
- IDE autocomplete and refactoring support is limited

## Evidence
**Files analyzed:**
- `tools/ble_serial.py`: 1 function with annotations (`on_notify`)
- `tools/flash_firmware.py`: 0 functions with annotations
- `tools/coredump.py`: 0 functions with annotations  
- `tools/pio_submodules.py`: 2 functions with annotations (`_is_populated`, `_update_submodules`)

**No type checker config found:**
- No `pyproject.toml` with `[tool.mypy]` or `[tool.pyright]`
- No `mypy.ini` or `setup.cfg`
- No `pyrightconfig.json`

**CI workflow check:**
- `.github/workflows/build.yml` has Python 3.11 setup but no type checking step
- `.github/workflows/deploy-pages.yml` has no Python step

## Recommended Fix
1. Install mypy: Add to `tools/requirements.txt`:
   ```
   mypy>=1.8.0
   ```

2. Create `pyproject.toml` in project root with mypy configuration:
   ```toml
   [tool.mypy]
   python_version = "3.11"
   warn_return_any = true
   warn_unused_configs = true
   disallow_untyped_defs = false  # Start lenient, tighten later
   check_untyped_defs = true
   ```

3. Add type checking step to `.github/workflows/build.yml`:
   ```yaml
   - name: Type check Python tools
     run: |
       pip install -r tools/requirements.txt
       mypy tools/*.py --exclude pio_component_manager.py  # SCons script
   ```

4. Add basic type annotations to existing functions (optional, incremental):
   - Start with return types for main functions
   - Add type hints to function parameters

## References
- [mypy documentation](https://mypy.readthedocs.io/)
- [Python typing cheatsheet](https://mypy.readthedocs.io/en/stable/cheat_sheet_py3.html)
- [PEP 484 - Type Hints](https://peps.python.org/pep-0484/)
