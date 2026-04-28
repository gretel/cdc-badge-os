---
title: "[HIGH] Missing manifest.json for Web Serial Flasher"
severity: HIGH
domain: cdc-badge-os
lens: session-lighthouse
labels:
  - "audit:toolgate/session-lighthouse"
---

## Summary
The web-flasher `index.html` references `manifest.json` (line 265) but the file does not exist in the `/web-flasher/` directory. This causes the `<esp-web-install-button>` component to fail silently or throw an error, breaking the core functionality of the web flasher.

**File:** `web-flasher/index.html:265`
**Element:** `<esp-web-install-button manifest="manifest.json">`

## Impact
- **Critical functionality broken**: Users cannot flash firmware without a valid manifest
- **Poor user experience**: Button may appear but fail when clicked with no clear error
- **Console errors**: Browser console will show "manifest.json not found" or similar

## Evidence
```bash
$ ls -la web-flasher/
total 19
-rw-------  7670 badge.jpg
-rw-r--r--  8077 index.html
# manifest.json is MISSING!
```

In `index.html:265`:
```html
<esp-web-install-button manifest="manifest.json">
```

The [esp-web-tools](https://esphome.github.io/esp-web-tools/) library requires a valid manifest file to know which firmware files to install.

## Recommended Fix
Create `web-flasher/manifest.json` with the following structure:

```json
{
  "name": "CDC Badge OS",
  "version": "1.0.0",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        {
          "path": "firmware.bin",
          "offset": 0
        }
      ]
    }
  ]
}
```

Alternatively, use the [ESPHome format](https://esphome.github.io/esp-web-tools/manifest/) if the firmware is split into multiple binary parts (bootloader, partitions, app):

```json
{
  "name": "CDC Badge OS",
  "version": "1.0.0",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "bootloader.bin", "offset": 0x0 },
        { "path": "partitions.bin", "offset": 0x8000 },
        { "path": "firmware.bin", "offset": 0x10000 }
      ]
    }
  ]
}
```

Then ensure the binary files are either:
1. Copied to the `web-flasher/` directory alongside `manifest.json`
2. Hosted from a known URL with full paths in the manifest

## References
- [esp-web-tools Manifest Format](https://esphome.github.io/esp-web-tools/manifest/)
- [ESP32 Flashing Overview](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/sd_card.html)
