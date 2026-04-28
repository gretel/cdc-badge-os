---
title: "[LOW] Python scripts lack type hints for function parameters"
severity: LOW
domain: lint
lens: toolgate/lint
labels:
  - "audit:toolgate/lint"
---

## Summary
Python functions use type hints in some places but inconsistently apply them throughout the codebase.

**Files affected:**
- `tools/flash_firmware.py` - Functions like `find_port()`, `find_bin_file()`, `resolve_binaries()` lack return type hints
- `tools/coredump.py` - Functions like `find_port()`, `run_command()` lack type hints

## Impact
Type hints improve code readability, IDE support, and help catch bugs through static analysis. Inconsistent use reduces these benefits.

## Evidence
**tools/flash_firmware.py (lines 44-51):**
```python
def find_port():
    """Auto-detect USB serial port."""
```

Should be:
```python
def find_port() -> str | None:
    """Auto-detect USB serial port."""
```

**tools/flash_firmware.py (lines 53-69):**
```python
def find_bin_file(directory, keyword):
    """Find a bin file in directory matching the keyword..."""
```

Should be:
```python
def find_bin_file(directory: str, keyword: str) -> str | None:
    """Find a bin file in directory matching the keyword..."""
```

**tools/coredump.py (lines 39-46):**
```python
def find_port():
    """Find USB serial port."""
```

Should be:
```python
def find_port() -> str | None:
    """Find USB serial port."""
```

## Recommended Fix
Add type hints to all function parameters and return values:

**tools/flash_firmware.py:**
```python
def find_port() -> str | None:
    """Auto-detect USB serial port."""

def find_bin_file(directory: str, keyword: str) -> str | None:
    """Find a bin file in directory matching the keyword."""

def resolve_binaries(directory: str) -> dict:
    """Resolve all 3 required binaries from a directory."""
```

**tools/coredump.py:**
```python
def find_port() -> str | None:
    """Find USB serial port."""

def run_command(cmd: list, description: str) -> bool:
    """Run a command and handle errors."""

def main() -> None:
    """Main entry point."""
```

## References
- Type Hints in Python: https://docs.python.org/3/library/typing.html
- PEP 484 - Type Hints: https://peps.python.org/pep-0484/
- mypy Type Checker: https://mypy.readthedocs.io/
