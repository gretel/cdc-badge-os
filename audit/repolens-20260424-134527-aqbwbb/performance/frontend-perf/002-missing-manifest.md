---
title: "[LOW] Missing manifest.json for esp-web-tools"
severity: LOW
domain: frontend-perf
lens: frontend-performance
labels:
  - "esp-web-tools"
  - "web-flasher"
---

## Summary
The `esp-web-install-button` component references `manifest="manifest.json"` but the file is missing from the web-flasher directory. This will cause the flasher to fail when users try to connect.

**Location:** `web-flasher/index.html:264`

## Impact
- The Web Serial flasher will not work without a valid manifest file
- Users will get an error when clicking the connect button
- Critical functionality is broken for the web flasher feature

## Evidence
```html
<!-- Line 264 -->
<esp-web-install-button manifest="manifest.json">
```

The `manifest.json` file should contain the firmware binary information but is missing from the web-flasher directory.

## Recommended Fix
Create `web-flasher/manifest.json` with the firmware binary reference:

```json
{
  "name": "CDC Badge OS",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "improv": true,
      "parts": [
        { "path": "firmware.bin", "offset": 0 }
      ]
    }
  ]
}
```

Note: The actual firmware binary path needs to be determined based on where the built firmware is deployed.

## References
- [esp-web-tools documentation](https://esphome.github.io/esp-web-tools/)
- [Manifest format](https://esphome.github.io/esp-web-tools/manifest/)
