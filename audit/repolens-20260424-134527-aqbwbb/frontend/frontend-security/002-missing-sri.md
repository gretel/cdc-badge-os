---
title: "[MEDIUM] Third-Party Script Without SRI Integrity Attribute"
severity: MEDIUM
domain: frontend-security
lens: sri-verification
labels:
  - "sri-missing"
  - "third-party-scripts"
  - "web-flasher"
---

## Summary
The Web Flasher loads the `esp-web-tools` library from unpkg.com CDN without Subresource Integrity (SRI) attributes. This means the browser will execute whatever script unpkg.com serves, with no verification that it hasn't been tampered with.

**Location:** `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html` (line 9)

## Impact
Without SRI, the page is vulnerable to:
- **CDN compromise**: If unpkg.com is compromised, malicious scripts execute
- **DNS hijacking**: If DNS is poisoned, a malicious server can serve different JS
- **Man-in-the-middle**: Without integrity check, modified scripts can be injected
- **Supply chain attack**: Version `@10` is a major version range, could include breaking changes

For a firmware flasher, this could result in:
- Flashing modified/fake firmware to devices
- Installing malware on hardware security keys
- Stealing device connection information

## Evidence
Current code (line 7-10):
```html
<script
  type="module"
  src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
></script>
```

Issues:
1. No `integrity` attribute with SHA-384/SHA-256 hash
2. No `crossorigin="anonymous"` attribute (required for SRI with CORS)
3. Uses major version `@10` which could resolve to any 10.x.x version

## Recommended Fix
Add SRI integrity attribute and crossorigin to the script tag:

1. First, get the current integrity hash:
   ```bash
   curl -s https://unpkg.com/esp-web-tools@10/dist/web/install-button.js | \
   openssl dgst -sha384 -binary | \
   openssl base64 -A
   ```

2. Update the script tag with integrity and crossorigin:
   ```html
   <script
     type="module"
     src="https://unpkg.com/esp-web-tools@10.2.1/dist/web/install-button.js?module"
     integrity="sha384-<paste-hash-here>"
     crossorigin="anonymous"
   ></script>
   ```

3. Best practices:
   - Pin to a specific version (e.g., `@10.2.1` not `@10`)
   - Update integrity when upgrading versions
   - Consider hosting the library locally for critical applications

## References
- [MDN: Subresource Integrity](https://developer.mozilla.org/en-US/docs/Web/Security/Subresource_Integrity)
- [OWASP: SRI Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Subresource_Integrity_Cheat_Sheet.html)
- [SRI Hash Generator](https://www.srihash.org/)
