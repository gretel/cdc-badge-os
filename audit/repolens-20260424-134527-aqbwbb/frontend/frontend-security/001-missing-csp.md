---
title: "[MEDIUM] Missing Content-Security-Policy (CSP) in Web Flasher"
severity: MEDIUM
domain: frontend-security
lens: csp-configuration
labels:
  - "csp-missing"
  - "xss-protection"
  - "web-flasher"
---

## Summary
The CDC Badge OS Web Flasher (`web-flasher/index.html`) lacks a Content-Security-Policy (CSP) meta tag or HTTP header. This means there is no restriction on where scripts, styles, and other resources can be loaded from, making the page vulnerable to XSS attacks.

**Location:** `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html` (lines 1-309)

## Impact
Without a CSP, the web flasher is vulnerable to:
- **Cross-Site Scripting (XSS)**: Any script injected into the page will execute
- **Data exfiltration**: Malicious scripts can read and send data to external servers
- **Clickjacking**: No `frame-ancestors` directive to prevent embedding in iframes
- **Unrestricted resource loading**: Scripts/styles can be loaded from any origin

Given that this tool is used to flash firmware to hardware security keys, XSS could potentially:
- Modify the firmware being flashed
- Steal device connection data
- Redirect users to malicious flasher pages

## Evidence
The `index.html` file contains:
- No `<meta name="Content-Security-Policy" ...>` tag in the `<head>`
- No `Content-Security-Policy` HTTP header configuration
- External script loaded from CDN without CSP restrictions:
  ```html
  <script type="module" src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>
  ```
- GitHub API fetch call without CSP allowlist:
  ```javascript
  const resp = await fetch("https://api.github.com/repos/krim404/cdc-badge-os/releases/latest");
  ```

## Recommended Fix
Add a CSP meta tag to the `<head>` section of `web-flasher/index.html`:

```html
<meta http-equiv="Content-Security-Policy" content="default-src 'self'; script-src 'self' https://unpkg.com; style-src 'self' 'unsafe-inline'; connect-src 'self' https://api.github.com; img-src 'self' data:;">
```

This CSP:
- Allows scripts only from self and unpkg.com (for esp-web-tools)
- Allows styles from self with inline styles (for the embedded CSS)
- Allows connections to self and GitHub API for version fetching
- Allows images from self and data: URIs

For production, consider:
1. Adding Specific integrity hashes (see related finding on SRI)
2. Using `script-src 'self' https://unpkg.com` with specific versions
3. Adding `frame-ancestors 'self'` to prevent clickjacking

## References
- [MDN: Content Security Policy (CSP)](https://developer.mozilla.org/en-US/docs/Web/HTTP/CSP)
- [CSP Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Content_Security_Policy_Cheat_Sheet.html)
- [OWASP: CSP Overview](https://owasp.org/www-community/Content_Security_Policy)
