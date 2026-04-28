---
title: "[MEDIUM] Python files missing shebang for scripts"
severity: MEDIUM
domain: lint
lens: toolgate/lint
labels:
  - "audit:toolgate/lint"
---

## Summary
Several Python files in the `tools/` directory are missing the shebang line (`#!/usr/bin/env python3`), making them non-executable as standalone scripts.

**Files affected:**
- `tools/pio_component_manager.py` - Line 1: starts with `Import("env")` instead of shebang

## Impact
Without a shebang, scripts cannot be run directly as executables (e.g., `./pio_submodules.py`). Users must explicitly call `python3 pio_submodules.py`, which is less convenient and less consistent with the other scripts in the project.

## Evidence
**tools/pio_component_manager.py (lines 1-4):**
```python
Import("env")

# Ensure ESP-IDF Component Manager is enabled for esp_tinyusb fetches.
env["ENV"]["IDF_COMPONENT_MANAGER"] = "1"
```

Compare with properly formed scripts:
- `tools/ble_serial.py` (line 1): `#!/usr/bin/env python3`
- `tools/coredump.py` (line 1): `#!/usr/bin/env python3`
- `tools/flash_firmware.py` (line 1): `#!/usr/bin/env python3`
- `tools/pio_submodules.py` (line 1): `#!/usr/bin/env python3`

## Recommended Fix
Add the shebang line at the top of `tools/pio_component_manager.py`:

```python
#!/usr/bin/env python3
Import("env")

# Ensure ESP-IDF Component Manager is enabled for esp_tinyusb fetches.
env["ENV"]["IDF_COMPONENT_MANAGER"] = "1"
```

Also ensure the file has executable permissions:
```bash
chmod +x tools/pio_component_manager.py
```

## References
- PEP 394 - Python Shebangs: https://peps.python.org/pep-0394/
- Python Scripting Best Practices: https://docs.python.org/3/tutorial/scripts.html
