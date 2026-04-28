---
title: "[MEDIUM] Missing security.txt file (RFC 9116 standard)"
severity: MEDIUM
domain: compliance
lens: security-disclosure
labels:
  - "missing-security-txt"
  - "rfc-9116"
---

## Summary
The repository lacks a `security.txt` file following RFC 9116 standard. This file should be placed in `.well-known/` directory and optionally served by the web flasher application. The web flasher at `web-flasher/index.html` is a public-facing component but has no security contact information.

**Files Affected:**
- `.well-known/security.txt` - Does not exist
- `web-flasher/index.html` - No security contact in footer (lines 285-288)
- Web application root - No security.txt served

## Impact
Without a `security.txt` file:
1. **RFC 9116 compliance** - Missing standard for security disclosure
2. **Discoverability** - Security researchers need to find contact info manually
3. **Web flasher exposure** - The web flasher is a public web application that could have vulnerabilities but no clear reporting path
4. **Inconsistent experience** - Different from industry standard practice (used by GitHub, Google, etc.)

## Evidence
```bash
# No .well-known directory
$ find . -path '*well-known/security.txt'
(no results)

# Web flasher footer has no security contact (lines 285-288)
<footer>
  <a href="https://github.com/krim404/cdc-badge-os">CDC Badge OS on GitHub</a>
</footer>
```

## Recommended Fix
Create a `.well-known/security.txt` file with the following content:

```
Contact: mailto:security@domain.com
Expires: 2026-12-31T23:59:59.000Z
Preferred-Languages: en, de
Canonical: https://github.com/krim404/cdc-badge-os/blob/main/.well-known/security.txt
Policy: https://github.com/krim404/cdc-badge-os/blob/main/SECURITY.md
```

Place the file in two locations:
1. **Repository root**: `.well-known/security.txt`
2. **Web flasher**: Copy to `web-flasher/.well-known/security.txt` (will be served at `https://krim404.github.io/cdc-badge-os/.well-known/security.txt`)

Required fields per RFC 9116:
- `Contact`: Email or URL for security reports
- `Expires`: Date when the file should be refreshed

Recommended fields:
- `Preferred-Languages`: Supported languages for reports
- `Canonical`: URL to the canonical location
- `Policy`: Link to full security policy

## References
- [RFC 9116 - security.txt](https://www.rfc-editor.org/rfc/rfc9116.html)
- [securitytxt.org](https://securitytxt.org/)
- [GitHub Security Advisories](https://docs.github.com/en/code-security/security-advisories)

---
**Related to issue #001** - Both issues should be resolved together as part of a complete vulnerability disclosure policy implementation.
