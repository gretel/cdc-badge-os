---
title: "[MEDIUM] No Full Chip Erase Option for Clean Re-flashing"
severity: MEDIUM
domain: deployment-safety
lens: deployment-safety
labels:
  - "audit:devops/deployment-safety"
---

## Summary
The flash tool (`flash_firmware.py`) only erases the NVS partition (0x9000, 20KB) but doesn't provide an option to fully erase the chip. This makes it difficult to recover from corrupted flash or perform a clean slate re-flash.

**Files:**
- `tools/flash_firmware.py` (lines 175-208)

## Impact
- **Recovery difficulty**: Corrupted flash beyond NVS requires manual esptool commands
- **Incomplete cleanup**: Old firmware remnants may remain in flash
- **Developer friction**: Flashing a clean build requires switching to esptool directly
- **Inconsistent state**: Partial erases may leave devices in undefined states

## Evidence
The flash function only offers NVS erase:
```python
# flash_firmware.py:192-203
if erase_nvs:
    print("\nErasing NVS partition (0x9000, 20KB)...")
    erase_cmd = [
        "--chip", CHIP,
        "--port", port,
        "--baud", str(BAUD),
        "erase_region", "0x9000", "0x5000",
    ]
    try:
        esptool.main(erase_cmd)
    except SystemExit:
        print("WARNING: NVS erase failed (non-critical)")
```

No full erase option:
```python
# flash_firmware.py:211-240
parser.add_argument("--erase-nvs", action="store_true",
                    help="Erase NVS partition after flashing (resets all settings)")
# No --erase-all option
```

The web flasher also doesn't support full erase:
```html
<!-- web-flasher/index.html:25-30 -->
<esp-web-install-button manifest="manifest.json">
```

## Recommended Fix
Add a `--erase-all` option to the flash tool:

```python
# Add to argument parser
parser.add_argument("--erase-all", action="store_true",
                    help="Full chip erase before flashing (use for recovery)")

# Add to flash function
if erase_all:
    print("\nErasing entire chip...")
    erase_cmd = [
        "--chip", CHIP,
        "--port", port,
        "--baud", str(BAUD),
        "erase_flash",
    ]
    esptool.main(erase_cmd)
```

Add documentation warning:
```markdown
**Warning**: `--erase-all` will wipe all data including attestation keys and settings.
Only use for recovery or before first-time setup.
```

## References
- esptool erase_flash: https://github.com/espressif/esptool/blob/master/README.md#erase_flash
- ESP32 flash recovery: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/tools/idf-flash.html#erase-flash
