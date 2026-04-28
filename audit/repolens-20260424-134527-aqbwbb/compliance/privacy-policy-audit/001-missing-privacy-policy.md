---
title: "[MEDIUM] Missing Privacy Policy for Web Flasher and Firmware Documentation"
severity: MEDIUM
domain: compliance
lens: privacy-policy-audit
labels:
  - "audit:compliance/privacy-policy-audit"
---

## Summary
The CDC Badge OS repository lacks a dedicated privacy policy document. While the project is primarily embedded firmware (which stores data locally on-device), the **web-flasher** component (`web-flasher/index.html`) is a web-based service that users interact with to flash firmware to their devices. This web interface makes API calls to GitHub and uses external CDN resources, constituting data processing that should be disclosed.

**Files analyzed:**
- `web-flasher/index.html` - Web flasher interface
- `README.md` - Main documentation
- `docs/` - Documentation directory
- Root directory (no `PRIVACY.md`, `PRIVACY_POLICY.md`, `DATA_PROTECTION.md`, or similar found)

## Impact
1. **Legal Compliance Risk**: Under GDPR (Art. 13-14) and similar privacy regulations, any web service that processes user data needs a privacy policy. The web flasher fetches version data from GitHub API and loads resources from external CDNs.

2. **User Transparency**: Users connecting via Web Serial API should be informed about what data is transmitted, what third-party services are used, and how long data is retained.

3. **GitHub Pages Deployment**: The web flasher is deployed at `https://krim404.github.io/cdc-badge-os/` - a public-facing web service that should have discoverable privacy information.

## Evidence
**Web Flasher Data Processing (`web-flasher/index.html`):**

1. **GitHub API call** (lines 285-298):
```javascript
const resp = await fetch(
  "https://api.github.com/repos/krim404/cdc-badge-os/releases/latest"
);
```

2. **External CDN resource** (lines 7-9):
```html
<script
  type="module"
  src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
></script>
```

3. **Web Serial API usage** - Users grant browser permission to access USB devices

**Firmware Data Processing (for reference):**
- `components/mod_totp/` - Stores TOTP account names, issuers, secrets (locally)
- `components/mod_password/` - Stores titles, usernames, passwords, URLs, notes (locally)
- `components/mod_fido2/` - Stores Relying Party IDs, credentials (locally)
- `components/mod_vcard/` - Stores/exchanges contact data via BLE
- `components/cdc_os_ui/src/WifiHandlers.cpp` (lines 233-234): Syncs time with external NTP servers (pool.ntp.org, time.google.com)

## Recommended Fix
Create a `PRIVACY.md` file in the repository root with the following sections:

1. **Scope**: Clarify that firmware stores data locally on-device (no cloud processing)
2. **Web Flasher**: Document third-party services used:
   - GitHub API for version fetching
   - unpkg.com CDN for esp-web-tools
   - Web Serial API for device communication
3. **NTP Sync**: Disclose external time servers (pool.ntp.org, time.google.com)
4. **BLE vCard**: Explain peer-to-peer contact exchange (no central server)
5. **Data Retention**: Explain that all user data stays on-device until manually deleted
6. **Contact**: Provide maintainer contact for privacy questions

Example structure:
```markdown
# Privacy Policy

## Overview
CDC Badge OS is firmware for a hardware security key...

## Web Flasher
The web flasher at https://krim404.github.io/cdc-badge-os/ uses:
- GitHub API to fetch latest release information
- unpkg.com CDN for esp-web-tools library

## Firmware Data Storage
All user data is stored locally on the device...

## External Connections
- NTP time sync: pool.ntp.org, time.google.com
- FIDO2: Data sent only to relying parties you authenticate with

## Contact
For privacy questions: [email or GitHub issues]
```

## References
- [GDPR Article 13](https://gdpr.eu/article-13-information-to-be-provided/) - Information to be provided
- [GDPR Article 14](https://gdpr.eu/article-14-information-to-be-provided/) - Information from another source
- [Web Serial API Privacy](https://web.dev/serial/) - Browser API privacy considerations
- [esp-web-tools](https://esphome.github.io/esp-web-tools/) - Third-party library documentation
