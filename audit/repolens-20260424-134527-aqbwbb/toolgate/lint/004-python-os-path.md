---
title: "[MEDIUM] Python scripts use deprecated `os.path` functions"
severity: MEDIUM
domain: lint
lens: toolgate/lint
labels:
  - "audit:toolgate/lint"
---

## Summary
Python scripts use `os.path` functions that have modern `pathlib` equivalents available in Python 3.6+.

**Files affected:**
- `tools/flash_firmware.py` (line 58, 133, 135)
- `tools/coredump.py` (line 63, 72)

## Impact
The `os.path` module is considered legacy. `pathlib` provides a more object-oriented and readable API. Using `pathlib` is the modern Python best practice.

## Evidence
**tools/flash_firmware.py (line 58):**
```python
for f in os.listdir(directory):
```

**tools/flash_firmware.py (line 133):**
```python
with open(dest, "wb") as f:
```

**tools/flash_firmware.py (line 135):**
```python
size_kb = os.path.getsize(path) / 1024
```

**tools/coredump.py (line 63):**
```python
if not os.path.exists(FIRMWARE_ELF):
```

## Recommended Fix
Replace `os.path` with `pathlib`:

**tools/flash_firmware.py:**
```python
from pathlib import Path

# Line 58
for f in Path(directory).iterdir():
    # f is a Path object, use f.name for filename

# Line 133
Path(dest).write_bytes(r.content)  # or keep open() for large files

# Line 135
size_kb = Path(path).stat().st_size / 1024
```

**tools/coredump.py:**
```python
from pathlib import Path

# Line 63
if not Path(FIRMWARE_ELF).exists():
```

## References
- pathlib documentation: https://docs.python.org/3/library/pathlib.html
- PEP 428 - The pathlib module: https://peps.python.org/pep-0428/
