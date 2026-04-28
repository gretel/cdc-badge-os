---
title: "[MEDIUM] External Script Loaded Without Subresource Integrity"
severity: MEDIUM
domain: security-headers
lens: security-headers
labels:
  - "audit:security/security-headers"
---

## Summary
The web-flasher at `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html` loads an external JavaScript file from unpkg.com without Subresource Integrity (SRI) attributes.

## Impact
Without SRI, if the CDN (unpkg.com) is compromised or the package version changes, malicious JavaScript could be executed in users' browsers. This could:
- Modify the flashing process to write malicious firmware
- Steal device data during the flashing process
- Inject additional scripts or modify the UI

## Evidence
File: `web-flasher/index.html`
- Line 7-10: External script without integrity attribute:
  ```html
  <script
    type="module"
    src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
  ></script>
  ```

## Recommended Fix
Add `integrity` and `crossorigin` attributes to the script tag:

1. First, get the SHA-384 hash of the current version:
   ```bash
   curl -s https://unpkg.com/esp-web-tools@10/dist/web/install-button.js | openssl dgst -sha384 -binary | openssl base64 -A
   ```

2. Update the script tag (line 7-10):
   ```html
   <script
     type="module"
     src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
     integrity="sha384-<hash-from-above>"
     crossorigin="anonymous"
   ></script>
   ```

Alternatively, consider:
- Downloading the script and hosting it locally
- Using a more specific version (e.g., `@10.1.2` instead of `@10`)

## References
- [MDN: Subresource Integrity](https://developer.mozilla.org/en-US/docs/Web/Security/Subresource_Integrity)
- [W3C: SRI Specification](https://www.w3.org/TR/SRI/)
- [esp-web-tools npm](https://www.npmjs.com/package/esp-web-tools)
