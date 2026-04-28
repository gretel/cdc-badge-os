---
title: "[LOW] Web Flasher references missing manifest.json file"
severity: LOW
domain: web-flasher
lens: xss-csrf
labels:
  - web-flasher
  - missing-file
  - esp-web-tools
---

## Summary
The web-flasher `index.html` references a `manifest.json` file that does not exist in the directory. This causes the `<esp-web-install-button>` component to potentially fail silently or display errors to users.

**File:** `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html`  
**Line:** 264  
**Context:** `<esp-web-install-button manifest="manifest.json">`

## Impact
- **User Experience:** Users clicking the flash button may encounter errors without clear feedback
- **Functionality:** The firmware flashing feature may not work correctly if the manifest is required
- **Minimal Security Risk:** This is primarily a functionality issue, not a direct XSS/CSRF vulnerability

## Evidence
```html
<!-- Line 264 -->
<esp-web-install-button manifest="manifest.json">
  <button slot="activate">Connect (Flash/Serial)</button>
  ...
</esp-web-install-button>
```

Directory listing shows `manifest.json` is missing:
```
/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/
├── badge.jpg
├── index.html
└── (manifest.json is missing)
```

## Recommended Fix
Create a `manifest.json` file in the web-flasher directory with the appropriate firmware manifest structure for esp-web-tools:

```json
{
  "name": "CDC Badge OS",
  "version": "1.0.0",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "firmware.bin", "offset": 0 }
      ]
    }
  ]
}
```

Alternatively, update the HTML to use a URL-based manifest if the firmware is hosted remotely:
```html
<esp-web-install-button manifest="https://example.com/manifest.json">
```

## References
- [ESP Web Tools Documentation](https://esphome.github.io/esp-web-tools/)
- [Web Serial API Flashing Guide](https://web.dev/serial/#flashing-firmware)
