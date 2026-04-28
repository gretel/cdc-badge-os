---
title: "[MEDIUM] Missing manifest.json for Web Flasher"
severity: MEDIUM
domain: frontend-security
lens: web-serial-configuration
labels:
  - "missing-file"
  - "web-serial"
  - "web-flasher"
---

## Summary
The Web Flasher references a `manifest.json` file (line 261) but this file does not exist in the web-flasher directory. The `esp-web-install-button` element requires a manifest to specify the firmware binary files and their addresses.

**Location:** `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html` (line 261)

## Impact
Without a manifest.json:
- **Web Serial won't work**: The flasher button will not function
- **User experience broken**: Users clicking "Connect (Flash/Serial)" will get an error
- **Firmware distribution**: Cannot properly specify which firmware files to flash

The manifest.json is required by esp-web-tools to:
1. Specify the firmware binary files and their flash addresses
2. Define the reset behavior after flashing
3. Configure the connection options

## Evidence
Line 261 in `index.html`:
```html
<esp-web-install-button manifest="manifest.json">
```

Checking the web-flasher directory:
```bash
$ ls -la web-flasher/
-rw-r--r-- 1 user user  index.html
-rw-r--r-- 1 user user  badge.jpg
# No manifest.json found!
```

The manifest is referenced but the file is missing.

## Recommended Fix
Create a `manifest.json` file in the `web-flasher/` directory with the following structure:

```json
{
  "name": "CDC Badge OS",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "improv": true,
      "parts": [
        {
          "file": "firmware.bin",
          "address": 0x0
        }
      ]
    }
  ]
}
```

For a complete setup:
1. Host the firmware binary alongside manifest.json
2. Update the `file` path to point to the actual firmware location
3. Consider adding version-specific manifests for different firmware releases
4. Test the flasher with `npx serve` or similar local server

Example with multiple partitions:
```json
{
  "name": "CDC Badge OS v1.0",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "improv": true,
      "parts": [
        { "file": "bootloader.bin", "address": 0x1000 },
        { "file": "partitions.bin", "address": 0x8000 },
        { "file": "firmware.bin", "address": 0x10000 }
      ]
    }
  ]
}
```

## References
- [esp-web-tools Manifest Format](https://esphome.github.io/esp-web-tools/#using-the-manifest)
- [ESP Web Tools Documentation](https://esphome.github.io/esp-web-tools/)
- [Firmware Binary Layout for ESP32](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/bin_file.html)
