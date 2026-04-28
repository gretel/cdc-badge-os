---
title: "[INFO] Firmware has no HTTP server - minimal XSS/CSRF attack surface"
severity: INFO
domain: firmware
lens: xss-csrf
labels:
  - firmware
  - architecture
  - attack-surface
---

## Summary
The CDC Badge OS firmware has **no HTTP server** and communicates exclusively through:
- USB CDC (serial console)
- USB HID (keyboard/smartcard modes)
- BLE (Bluetooth Low Energy)

This architecture means traditional XSS (Cross-Site Scripting) and CSRF (Cross-Site Request Forgery) vulnerabilities do not apply to the firmware itself.

**Files examined:** All firmware source files in `/components/`, `/main/`, `/include/`

## Impact
**Positive Security Implications:**
- **No HTTP-based XSS:** No web UI to inject scripts into
- **No CSRF:** No HTTP endpoints to forge requests against
- **Reduced attack surface:** Serial and BLE protocols don't have the same trust model as HTTP

**Note:** This is an **architectural observation**, not a vulnerability. The firmware design inherently avoids the most common web application vulnerabilities.

## Evidence

### Communication Protocols

**USB CDC (Serial Console)**
- File: `components/usb_badge/usb_cdc.cpp`
- Lines 180-220: Simple byte-stream read/write
- No protocol parsing that could interpret HTML/JavaScript

**USB HID (Keyboard/Smartcard)**
- File: `components/usb_badge/usb_hid.cpp`
- Emulates standard HID devices
- No web context

**BLE (Bluetooth Low Energy)**
- File: `components/cdc_hal/src/BleAdvParser.cpp`
- Lines 1-177: Parses BLE advertising data
- Data is binary, not HTML/JavaScript

### Serial Command Processing
- File: `components/serial_cmd/src/SerialCmd.cpp`
- Commands are parsed as text lines (e.g., `HELP`, `STATUS`, `AUTH <pin>`)
- Output is plain text to serial console, not HTML
- Example (line 415): `Console::printf("PONG\r\n");`

### Display Rendering
- File: `components/cdc_hal/include/cdc_hal/IDisplay.h`
- Lines 138-147: Text rendering methods (`print()`, `printf()`)
- Output goes to E-Paper display, not a web browser

## Recommended Actions

### For Current Architecture (No Changes Needed)
The current design is secure by default. No action required for XSS/CSRF.

### If Adding Web Features in Future
If a web interface is added later (e.g., over WiFi or USB), implement:

1. **XSS Protection**
   - Use template engines with auto-escaping
   - Context-appropriate output encoding (HTML, JS, URL, CSS)
   - Content-Security-Policy headers

2. **CSRF Protection**
   - CSRF tokens for state-changing operations
   - SameSite cookie attributes
   - Origin/Referer header validation

3. **Secure Defaults**
   ```
   Content-Security-Policy: default-src 'self';
   X-Content-Type-Options: nosniff
   X-Frame-Options: DENY
   ```

## Related Findings
- **002-missing-csp.md:** Web-flasher needs CSP (applies to the web page, not firmware)
- **003-cdn-integrity.md:** Web-flasher CDN resources need SRI

## References
- [OWASP XSS Prevention Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Cross_SiteScripting_Prevention_Cheat_Sheet.html)
- [OWASP CSRF Prevention Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/CSRF_Prevention_Cheat_Sheet.html)
- [USB CDC Specification](https://www.usb.org/document-library/device-class-definition-hub-11)
- [BLE Core Specification](https://www.bluetooth.com/bluetooth-resources/)

## Conclusion
The firmware architecture is **inherently protected** from XSS/CSRF due to its lack of an HTTP stack. The only web component is the web-flasher (a single static HTML page), which has its own set of findings (001-005).

</content>