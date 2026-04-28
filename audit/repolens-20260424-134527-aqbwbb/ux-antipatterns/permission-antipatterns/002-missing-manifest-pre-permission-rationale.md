---
title: "[MEDIUM] Missing manifest.json and minimal pre-permission rationale for Web Serial"
severity: MEDIUM
domain: web-flasher
lens: permission-antipatterns
labels:
  - "web-flasher"
  - "web-serial"
  - "pre-permission-rationale"
---

## Summary

The web-flasher component (`web-flasher/index.html`) uses the `esp-web-tools` custom element to request Web Serial permission but lacks a dedicated `manifest.json` file and provides minimal contextual explanation before the browser permission dialog fires. The Web Serial permission is triggered when the user clicks the "Connect (Flash/Serial)" button, but there is no intermediate "soft ask" modal or contextual UI that explains why the application needs serial access before triggering the native browser prompt.

**File:** `web-flasher/index.html`
**Lines:** 260-268

## Impact

Users clicking the connect button are immediately presented with the browser's native Web Serial permission dialog without understanding:
1. Why the badge needs serial connection
2. What the connection will be used for (firmware flashing)
3. What to expect after granting permission

This can lead to users reflexively denying the permission due to lack of context, especially first-time users who may not understand what "Web Serial" means or why a badge needs it. The missing `manifest.json` also means the esp-web-tools component may fail silently or show a generic error.

## Evidence

The `esp-web-install-button` element triggers Web Serial permission on click:

```html
<!-- Lines 260-268 -->
<esp-web-install-button manifest="manifest.json">
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
</esp-web-install-button>
```

The button text "Connect (Flash/Serial)" provides some context, but the native browser permission dialog appears immediately on click without any intermediate explanation modal.

Additionally, the `manifest.json` file referenced on line 261 is missing from the `web-flasher/` directory (verified by file listing).

## Recommended Fix

1. **Create manifest.json**: Add a `manifest.json` file in `web-flasher/` with the firmware manifest details for esp-web-tools:

```json
{
  "name": "CDC Badge OS",
  "version": "latest",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "improv": true,
      "parts": [
        { "path": "cdc-badge-os.bin", "offset": 0 }
      ]
    }
  ]
}
```

2. **Add pre-permission rationale modal**: Before the browser permission dialog fires, show an intermediate modal explaining:
   - What Web Serial is (a browser feature to connect to USB devices)
   - Why it's needed (to flash firmware to the CDC Badge)
   - What happens next (badge detection, firmware upload progress)
   - How to ensure the badge is in bootloader mode

Example implementation:

```html
<esp-web-install-button manifest="manifest.json">
  <button slot="activate">Connect (Flash/Serial)</button>
  <!-- Add a before-connect explanation -->
  <div slot="before-connect">
    <h3>Connect Badge via Web Serial</h3>
    <p>This will open a browser dialog to grant permission for serial access.</p>
    <p><strong>Why needed:</strong> To flash firmware to your CDC Badge via USB.</p>
    <p><strong>Make sure:</strong> Badge is in bootloader mode (FLASH+RESET).</p>
  </div>
  <!-- existing slots... -->
</esp-web-install-button>
```

3. **Enhance denial guidance**: The current denial message ("Serial access was denied. Please grant permission and try again.") could be more helpful by adding instructions on how to re-enable permissions in browser settings.

## References

- [Web Serial API - MDN](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API)
- [esp-web-tools documentation](https://espressif.github.io/esp-web-tools/)
- [Permission Request UX Best Practices](https://developers.google.com/web/updates/2019/08/consent-ux)
- [W3C Web Serial API Specification](https://wicg.github.io/web-serial/)
