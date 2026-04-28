---
title: "[HIGH] Web Flasher Missing Security Headers"
severity: HIGH
domain: security-headers
lens: security-headers
labels:
  - "audit:security/security-headers"
---

## Summary
The web-flasher `index.html` file at `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html` is a static HTML page that will be served via HTTP(S) but lacks essential security headers when deployed.

The page is referenced in the manifest at line 29:
```html
<esp-web-install-button manifest="manifest.json">
```

## Impact
Without security headers, the web flasher is vulnerable to:
- **Clickjacking attacks**: No `X-Frame-Options` or `frame-ancestors` directive
- **XSS attacks**: No Content-Security-Policy to restrict script sources
- **MIME sniffing attacks**: No `X-Content-Type-Options` header
- **Referrer leakage**: No `Referrer-Policy` to control URL referrer information

## Evidence
File: `web-flasher/index.html`
- Line 1-290: Complete HTML document with no `<meta http-equiv>` security headers
- Line 7-10: External script loaded from CDN without integrity attribute:
  ```html
  <script
    type="module"
    src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
  ></script>
  ```
- No `<meta http-equiv="Content-Security-Policy">` tag
- No `<meta http-equiv="X-Frame-Options">` tag
- No `<meta http-equiv="X-Content-Type-Options">` tag
- No `<meta http-equiv="Referrer-Policy">` tag

## Recommended Fix
Add security meta tags to the `<head>` section of `index.html` (after line 5):

```html
<meta http-equiv="Content-Security-Policy" content="default-src 'self'; script-src 'self' https://unpkg.com; style-src 'self' 'unsafe-inline'; img-src 'self' data:;">
<meta http-equiv="X-Frame-Options" content="DENY">
<meta http-equiv="X-Content-Type-Options" content="nosniff">
<meta http-equiv="Referrer-Policy" content="strict-origin-when-cross-origin">
```

**Note**: For production deployment, serve these headers from the web server (nginx/Apache/GitHub Pages config) rather than meta tags for stronger enforcement.

## References
- [MDN: HTTP headers for content security](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers)
- [OWASP Security Headers](https://owasp.org/www-project-secure-headers/)
- [Content Security Policy Guide](https://web.dev/articles/csp)
