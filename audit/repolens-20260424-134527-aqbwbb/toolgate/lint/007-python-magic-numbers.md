---
title: "[LOW] Python scripts use magic numbers for flash addresses"
severity: LOW
domain: lint
lens: toolgate/lint
labels:
  - "audit:toolgate/lint"
---

## Summary
Flash memory addresses are hardcoded as magic numbers instead of being defined as named constants.

**Files affected:**
- `tools/flash_firmware.py` (lines 28-33)
- `tools/coredump.py` (lines 23-24)

## Impact
Magic numbers make the code harder to read and maintain. Named constants provide self-documentation and make it easier to update values in one place.

## Evidence
**tools/flash_firmware.py (lines 28-33):**
```python
# Flash layout (address -> filename pattern)
FLASH_MAP = {
    0x0:     "bootloader",
    0x8000:  "partitions",
    0x10000: "firmware",
}
```

**tools/coredump.py (lines 23-24):**
```python
COREDUMP_OFFSET = 0xFF0000
COREDUMP_SIZE = 0x10000
```

The `0x8000` and `0x10000` values are magic numbers that could benefit from named constants.

## Recommended Fix
Define named constants for commonly used flash addresses:

**tools/flash_firmware.py:**
```python
# Flash memory layout - named constants for readability
FLASH_ADDR_BOOTLOADER = 0x0
FLASH_ADDR_PARTITIONS = 0x8000
FLASH_ADDR_FIRMWARE = 0x10000

# Flash layout (address -> filename pattern)
FLASH_MAP = {
    FLASH_ADDR_BOOTLOADER: "bootloader",
    FLASH_ADDR_PARTITIONS: "partitions",
    FLASH_ADDR_FIRMWARE: "firmware",
}
```

**tools/coredump.py:**
```python
# Flash memory layout
FLASH_ADDR_COREDUMP = 0xFF0000
FLASH_SIZE_COREDUMP = 0x10000

COREDUMP_OFFSET = FLASH_ADDR_COREDUMP
COREDUMP_SIZE = FLASH_SIZE_COREDUMP
```

## References
- Magic Numbers in Python: https://realpython.com/magic-numbers-python/
- Constants in Python: https://peps.python.org/pep-0008/#constants
