---
title: "[HIGH] Missing manifest.json for Web Flasher"
severity: HIGH
domain: frontend
lens: routing
labels:
  - "audit:frontend/routing"
---

## Summary
The web-flasher `index.html` references a `manifest.json` file that does not exist in the `/web-flasher/` directory. The ESP Web Tools install button requires this manifest to know which firmware files to flash.

**Location:** `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html:266`

```html
<esp-web-install-button manifest="manifest.json">
```

## Impact
- **Critical functionality broken**: Users cannot flash firmware via the web interface
- The `esp-web-install-button` component will fail to initialize properly
- Users clicking the flash button will see an error or nothing happening
- The main purpose of the web-flasher page is defeated

## Evidence
1. Line 266 in `web-flasher/index.html` references `manifest="manifest.json"`
2. Running `ls -la web-flasher/` shows only these files:
   - `index.html`
   - `badge.jpg`
   - (hidden files: `._index.html`, `._badge.jpg`)
3. No `manifest.json` file exists in the directory

## Recommended Fix
Create a `manifest.json` file in the `web-flasher/` directory with the following structure:

```json
{
  "name": "CDC Badge OS",
  "version": "latest",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "firmware.bin", "offset": 0x0 }
      ]
    }
  ]
}
```

The exact structure should match the ESP Web Tools manifest format. The `firmware.bin` path should point to the actual firmware file location (may need to be hosted separately or embedded).

**Alternative:** If firmware files are not yet available for web flashing, add a clear message to the UI indicating the feature is coming soon and disable the flash button.

## References
- [ESP Web Tools Documentation - Manifest](https://esphome.github.io/esp-web-tools/)
- [Install Button Manifest Format](https://esphome.github.io/esp-web-tools/manifest/)
