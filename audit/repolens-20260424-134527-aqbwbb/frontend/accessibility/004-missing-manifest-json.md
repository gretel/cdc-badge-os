---
title: "[HIGH] Referenced manifest.json file missing - flasher may not work"
severity: HIGH
domain: frontend
lens: a11y
labels:
  - "audit:frontend/accessibility"
---

## Summary
The web flasher references a `manifest.json` file that does not exist in the `web-flasher/` directory. This is critical because the `esp-web-install-button` component requires this manifest to know which firmware to flash. Users will not be able to flash the badge without this file.

**Location:** `web-flasher/index.html`, line 263
**Missing file:** `web-flasher/manifest.json`

```html
<esp-web-install-button manifest="manifest.json">
  <button slot="activate">Connect (Flash/Serial)</button>
  <!-- ... -->
</esp-web-install-button>
```

## Impact
- **Critical functionality broken** - The flasher will not work without the manifest
- **Users cannot flash firmware** - Primary purpose of the page fails silently
- **Poor user experience** - Button may appear but clicking it will show an error or do nothing
- **Accessibility impact** - Screen reader users may not understand why the button doesn't work

## Evidence
- Line 263: `<esp-web-install-button manifest="manifest.json">`
- File listing shows `manifest.json` does not exist in `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/`
- Only files present: `index.html`, `badge.jpg`, and Mac resource fork files (`._*`)

## Recommended Fix
Create the missing `manifest.json` file with proper firmware configuration:

```json
{
  "name": "CDC Badge OS",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "improv": true,
      "parts": [
        {
          "path": "CDC-Badge-OS.bin",
          "address": 0
        }
      ]
    }
  ]
}
```

Additional steps:
1. Ensure the firmware binary (`CDC-Badge-OS.bin`) is available at the same URL or relative path
2. Test the flasher in Chrome/Edge to verify it works
3. Add error handling for missing manifest in the HTML

Example with error handling:
```html
<esp-web-install-button manifest="manifest.json" id="flash-button">
  <button slot="activate">Connect (Flash/Serial)</button>
  <span slot="unsupported">
    <div class="browser-warning" style="display: block;">
      Your browser does not support Web Serial.<br>
      Please use <strong>Google Chrome</strong> or <strong>Microsoft Edge</strong> (desktop).
    </div>
  </span>
  <span slot="not-allowed">
    <div class="browser-warning" style="display: block;">
      Serial access was denied. Please grant permission and try again.
    </div>
  </span>
  <span slot="failed">
    <div class="browser-warning" style="display: block;">
      Failed to load manifest. Please refresh the page.
    </div>
  </span>
</esp-web-install-button>
```

## References
- [esp-web-tools Documentation](https://esphome.github.io/esp-web-tools/)
- [Web Serial API - Manifest format](https://web.dev/serial/#manifest)
- [ESPHome Web Installer](https://esphome.github.io/esp-web-tools/dist/web/install-button/)
