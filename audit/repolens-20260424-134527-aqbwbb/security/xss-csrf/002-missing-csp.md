---
title: "[MEDIUM] Web Flasher missing Content-Security-Policy (CSP)"
severity: MEDIUM
domain: web-flasher
lens: xss-csrf
labels:
  - web-flasher
  - csp
  - xss-protection
  - security-headers
---

## Summary
The web-flasher `index.html` lacks a Content-Security-Policy (CSP) meta tag or HTTP header. This makes the page more vulnerable to XSS attacks, especially since it loads external resources from CDN (unpkg.com) and fetches data from GitHub API.

**File:** `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html`  
**Location:** `<head>` section (lines 3-10)

## Impact
- **XSS Vulnerability:** Without CSP, if an attacker can inject JavaScript into the page (via GitHub API response, CDN compromise, or other vectors), it will execute without restriction
- **Data Exfiltration:** Malicious scripts could read the version data and send it elsewhere
- **Supply Chain Risk:** The page loads `esp-web-tools` from unpkg.com; if compromised, malicious firmware could be flashed

## Evidence
The page uses external resources without CSP protection:

```html
<!-- Line 7-10: External module from CDN -->
<script
  type="module"
  src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
></script>
```

```javascript
// Lines 291-307: Fetches data from GitHub API
async function fetchLatestVersion() {
  const badge = document.getElementById("version-badge");
  try {
    const resp = await fetch(
      "https://api.github.com/repos/krim404/cdc-badge-os/releases/latest"
    );
    const data = await resp.json();
    badge.textContent = data.tag_name;  // Safe: uses textContent
    ...
  }
}
```

No CSP meta tag is present in the `<head>` section.

## Recommended Fix
Add a Content-Security-Policy meta tag to the `<head>` section:

```html
<meta http-equiv="Content-Security-Policy" content="
  default-src 'self';
  script-src 'self' https://unpkg.com;
  style-src 'self';
  connect-src 'self' https://api.github.com;
  img-src 'self' data:;
  font-src 'self';
  object-src 'none';
  frame-ancestors 'self';
">
```

For better security, consider using Strict-Transport-Security as well:
```html
<meta http-equiv="Strict-Transport-Security" content="max-age=31536000; includeSubDomains">
```

If deploying with a web server, prefer HTTP headers over meta tags:
```
Content-Security-Policy: default-src 'self'; script-src 'self' https://unpkg.com; connect-src 'self' https://api.github.com;
```

## References
- [Content-Security-Policy MDN](https://developer.mozilla.org/en-US/docs/Web/HTTP/CSP)
- [CSP Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Content_Security_Policy_Cheat_Sheet.html)
- [OWASP XSS Prevention](https://cheatsheetseries.owasp.org/cheatsheets/Cross_SiteScripting_Prevention_Cheat_Sheet.html)
