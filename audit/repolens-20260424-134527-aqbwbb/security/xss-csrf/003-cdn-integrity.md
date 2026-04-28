---
title: "[MEDIUM] Web Flasher loads CDN resource without Subresource Integrity (SRI)"
severity: MEDIUM
domain: web-flasher
lens: xss-csrf
labels:
  - web-flasher
  - sri
  - cdn
  - supply-chain
---

## Summary
The web-flasher loads `esp-web-tools` from unpkg.com without Subresource Integrity (SRI) hashes. This makes the page vulnerable to supply-chain attacks where the CDN could be compromised or return modified JavaScript.

**File:** `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html`  
**Line:** 7-10

## Impact
- **Supply Chain Attack:** If unpkg.com is compromised or a specific version is updated with malicious code, users will execute the malicious JavaScript
- **Firmware Flashing:** Since this controls firmware flashing, a compromised script could flash malicious firmware to devices
- **No Version Pinning:** Uses `@10` which could auto-update to a new major version with breaking changes or vulnerabilities

## Evidence
```html
<!-- Lines 7-10: No integrity attribute -->
<script
  type="module"
  src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
></script>
```

The `integrity` attribute with SHA-384 or SHA-256 hash is missing.

## Recommended Fix
Add Subresource Integrity (SRI) attribute with a specific version:

1. First, download the specific version and generate the hash:
```bash
curl -s https://unpkg.com/esp-web-tools@10.0.0/dist/web/install-button.js | openssl dgst -sha384 -binary | openssl base64 -A
```

2. Update the HTML with pinned version and integrity:
```html
<script
  type="module"
  src="https://unpkg.com/esp-web-tools@10.0.0/dist/web/install-button.js?module"
  integrity="sha384-XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
  crossorigin="anonymous"
></script>
```

**Alternative (Better):** Host the library locally:
```html
<script type="module" src="esp-web-tools.js"></script>
```

This eliminates CDN dependency entirely and allows full control over the version.

## References
- [Subresource Integrity MDN](https://developer.mozilla.org/en-US/docs/Web/Security/Subresource_Integrity)
- [OWASP SRI](https://cheatsheetseries.owasp.org/cheatsheets/Testing_for_Subresource_Integrity.html)
- [SRI Hash Generator](https://www.srihash.org/)
