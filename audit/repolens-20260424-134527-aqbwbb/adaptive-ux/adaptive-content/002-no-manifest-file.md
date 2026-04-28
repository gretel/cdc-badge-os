---
title: "[HIGH] Referenced manifest.json file is missing from web-flasher"
severity: HIGH
domain: web-flasher
lens: adaptive-content
labels:
  - "missing-file"
  - "broken-reference"
---

## Summary
The `esp-web-install-button` component in `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html` (line 261) references `manifest="manifest.json"`, but the `manifest.json` file does not exist in the web-flasher directory. This breaks the core functionality of the web flasher.

**Evidence:**
```html
<esp-web-install-button manifest="manifest.json">
```

Directory listing shows only:
- `index.html`
- `badge.jpg`
- `._index.html` (macOS resource fork)
- `._badge.jpg` (macOS resource fork)

No `manifest.json` file present.

## Impact
- **Critical functionality broken**: The Web Serial installer cannot work without a valid manifest
- **User experience**: Users clicking the flash button will see an error or nothing happen
- **Deployment readiness**: The web flasher is incomplete and cannot be used in production

## Evidence
- File: `web-flasher/index.html`
- Line: 261
- Command output: `ls -la web-flasher/` shows no `manifest.json`

## Recommended Fix
Create a `manifest.json` file with the appropriate Web Manifest format for esp-web-tools:

```json
{
  "name": "CDC Badge OS",
  "version": "1.0.0",
  "builds": [
    {
      "data": "path/to/firmware.bin",
      "chip": "esp32s3",
      "erase": true
    }
  ]
}
```

Or if using a pre-built manifest from the firmware build:
1. Locate the firmware binary (likely in `.pio/build/cdc_badge_usb/`)
2. Create a proper esp-web-tools manifest pointing to it
3. Verify the manifest loads correctly in the browser console

## References
- [esp-web-tools Manifest format](https://espressif.github.io/esp-web-tools/manifest/)
- [Web Serial API requirements](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API)
