---
title: "[MEDIUM] Missing Web Flasher Manifest File"
severity: MEDIUM
domain: security-headers
lens: security-headers
labels:
  - "audit:security/security-headers"
---

## Summary
The web-flasher at `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html` references `manifest.json` (line 268) but the file does not exist in the `web-flasher/` directory.

## Impact
- The web flasher will fail to load the firmware manifest
- Users will see an error when trying to flash the device
- The `esp-web-tools` library needs the manifest to know which firmware file to flash

## Evidence
File: `web-flasher/index.html`
- Line 268: `<esp-web-install-button manifest="manifest.json">`

File: `web-flasher/` directory
- Contains: `index.html`, `badge.jpg`
- Missing: `manifest.json`

## Recommended Fix
Create `web-flasher/manifest.json` with the firmware manifest for esp-web-tools:

```json
{
  "name": "CDC Badge OS",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "improv": true,
      "parts": [
        { "path": "cdc-badge-os-firmware.bin", "offset": 0 }
      ]
    }
  ]
}
```

Or for a complete partition table flash:
```json
{
  "name": "CDC Badge OS",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "improv": true,
      "parts": [
        { "path": "bootloader.bin", "offset": 0x0 },
        { "path": "partition-table.bin", "offset": 0x8000 },
        { "path": "cdc-badge-os.bin", "offset": 0x10000 }
      ]
    }
  ]
}
```

Update the paths based on actual firmware file names from the build output.

## References
- [esp-web-tools Manifest Format](https://esphome.github.io/esp-web-tools/manifest/)
- [ESP32 Firmware Partitioning](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/partition-tables.html)
