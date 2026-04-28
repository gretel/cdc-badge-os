---
title: "[INFO] XSS/CSRF Audit Summary - Minimal web attack surface"
severity: INFO
domain: overall
lens: xss-csrf
labels:
  - audit-summary
  - firmware
  - web-flasher
---

## Summary
This audit examined the CDC Badge OS repository for XSS (Cross-Site Scripting) and CSRF (Cross-Site Request Forgery) vulnerabilities. The codebase is primarily **embedded firmware** for an ESP32-S3 hardware security key, with a minimal web component (web-flasher).

## Attack Surface Analysis

### Firmware (No HTTP Server)
The firmware itself has **no HTTP server**, **no web UI**, and communicates via:
- USB CDC (serial)
- USB HID (keyboard/smartcard)
- BLE (Bluetooth Low Energy)

**XSS/CSRF Risk:** N/A - These are not web protocols.

### Web Flasher (Single HTML Page)
The only web component is `/web-flasher/index.html`:
- **Static HTML** with inline CSS and JavaScript
- **No form handling** or user input rendering
- **No DOM manipulation** of user-controlled data
- Uses `textContent` (safe) instead of `innerHTML` (risky)
- Fetches version data from GitHub API
- Loads esp-web-tools from unpkg.com CDN

## Findings Overview

| # | Finding | Severity | File |
|---|---------|----------|------|
| 1 | Missing manifest.json | LOW | web-flasher/index.html |
| 2 | Missing Content-Security-Policy | MEDIUM | web-flasher/index.html |
| 3 | CDN resource without SRI | MEDIUM | web-flasher/index.html |

## Recommendations

### Immediate (Low Effort)
1. **Add CSP meta tag** to web-flasher
2. **Add SRI integrity** to esp-web-tools script tag
3. **Create manifest.json** for esp-web-tools

### Production Build
1. Ensure `DEBUG_MODE=0` for production
2. Ensure `FEATURE_SECURE_SERIAL=1` for production
3. Consider hosting esp-web-tools locally to eliminate CDN dependency

### Future Considerations
If a web interface is added to the firmware:
- Implement CSRF tokens for state-changing operations
- Add proper CORS configuration
- Set secure cookie attributes (HttpOnly, Secure, SameSite)
- Implement input sanitization for any HTML rendering
- Add Content-Security-Policy headers

## Conclusion
The codebase has **minimal XSS/CSRF risk** due to its architecture:
- No HTTP server in firmware
- Simple static web page with minimal JavaScript
- No user input rendered to HTML

The identified issues are mostly related to **defense-in-depth** and **supply-chain security** rather than direct XSS/CSRF vulnerabilities.

## References
- [OWASP XSS Prevention Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Cross_SiteScripting_Prevention_Cheat_Sheet.html)
- [OWASP CSRF Prevention Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/CSRF_Prevention_Cheat_Sheet.html)
- [Content-Security-Policy MDN](https://developer.mozilla.org/en-US/docs/Web/HTTP/CSP)
