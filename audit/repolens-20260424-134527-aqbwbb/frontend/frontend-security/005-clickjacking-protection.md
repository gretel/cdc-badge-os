---
title: "[MEDIUM] Missing frame-ancestors Directive for Clickjacking Protection"
severity: MEDIUM
domain: frontend-security
lens: clickjacking-protection
labels:
  - "clickjacking"
  - "frame-ancestors"
  - "web-flasher"
---

## Summary
The Web Flasher (`web-flasher/index.html`) has no `frame-ancestors` directive to prevent clickjacking attacks. Without this protection, the page can be embedded in an iframe on a malicious site, potentially tricking users into flashing firmware without their full awareness.

**Location:** `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html` (lines 1-309)

## Impact
Without `frame-ancestors` directive, the page is vulnerable to:
- **Clickjacking**: Malicious sites can embed the flasher in invisible iframes
- **UI overlay attacks**: Attackers can layer transparent buttons over the flasher
- **Covert firmware flashing**: Users might click "Connect" without realizing they're on an embedded flasher
- **State manipulation**: Attackers can programmatically trigger flasher actions via JavaScript

For a hardware security key flasher, this could result in:
- Flashing firmware without user's full knowledge
- Tricking users into connecting to malicious serial devices
- Subtle modifications to the flashing process via overlay

## Evidence
The `index.html` file (lines 1-309) contains:
- No `<meta name="Content-Security-Policy" ...>` tag
- No `Content-Security-Policy` HTTP header with `frame-ancestors`
- No `<meta http-equiv="X-Frame-Options" ...>` tag

Current structure (lines 1-21):
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>CDC Badge OS - Web Flasher</title>
  <script
    type="module"
    src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
  ></script>
```

No CSP or X-Frame-Options defined.

## Recommended Fix
Add a `frame-ancestors` directive to the CSP. Since the page uses inline styles and a module script, update the CSP meta tag:

```html
<meta http-equiv="Content-Security-Policy" content="default-src 'self'; script-src 'self' https://unpkg.com; style-src 'self' 'unsafe-inline'; connect-src 'self' https://api.github.com; img-src 'self' data:; frame-ancestors 'self';">
```

**Key points:**
1. `frame-ancestors 'self'` allows only same-origin iframes (most restrictive)
2. Use `frame-ancestors 'none'` for maximum protection (no iframes at all)
3. Use `frame-ancestors https://example.com` to allow specific origins

For the Web Flasher, `'self'` is appropriate since it's typically used standalone.

Alternative: Add X-Frame-Options meta tag (older browsers):
```html
<meta http-equiv="X-Frame-Options" content="SAMEORIGIN">
```

## References
- [MDN: frame-ancestors](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Content-Security-Policy/frame-ancestors)
- [MDN: X-Frame-Options](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/X-Frame-Options)
- [OWASP: Clickjacking](https://cheatsheetseries.owasp.org/cheatsheets/Clickjacking_Defense_Cheat_Sheet.html)
- [CSS-Tricks: frame-ancestors](https://css-tricks.com/frame-ancestors-and-x-frame-options/)
