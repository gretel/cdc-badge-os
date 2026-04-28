---
title: "[INFO] Cookie Policy Audit Complete - Not Applicable"
severity: INFO
domain: compliance
lens: cookie-policy
labels:
  - "audit:compliance/cookie-policy"
---

## Summary

The cookie policy audit was performed on the CDC Badge OS repository. The project is a **firmware project** (C/C++ for ESP32-S3 microcontroller) for a hardware security key, not a web service.

### Web Content Found

1. **Web Flasher** (`web-flasher/index.html`): A simple static page for flashing firmware via Web Serial API. Uses esp-web-tools from CDN. No cookies or localStorage are set by the project.

2. **Doxygen Documentation** (`doxygen_output/html/cookie.js`): Auto-generated documentation with a cookie.js file for storing user preferences (dark mode, search settings). This is standard Doxygen documentation infrastructure, not a web service.

## Impact

Cookie policy requirements apply to web services that set cookies or use tracking technologies for visitors. Since this project is primarily firmware with minimal static web content, a formal cookie policy is not required.

## Evidence

- Project type: Firmware (C/C++, ESP32-S3, PlatformIO)
- Web content: Static HTML page only (`web-flasher/index.html`)
- No server-side code that sets cookies
- No analytics scripts (Google Analytics, Mixpanel, etc.)
- No third-party tracking pixels
- localStorage usage only in auto-generated documentation (doxygen_output/html/cookie.js)

## Recommended Fix

No action required. The project does not meet the criteria for requiring a cookie policy:
- Not a web service
- No tracking cookies
- No analytics scripts
- No third-party tracking

If the project adds web-based features in the future (e.g., a web dashboard, admin panel), a cookie policy audit should be performed at that time.

## References

- [TTDSG (Telemediengesetz)](https://www.germany.eu/portal/en/features/19854.html) - German Telemedia Act
- [ePrivacy Directive](https://ec.europa.eu/info/law/law-topic/data-protection/eu-data-protection-essentials_en) - EU cookie law
- [GDPR](https://gdpr.eu/) - General Data Protection Regulation

DONE