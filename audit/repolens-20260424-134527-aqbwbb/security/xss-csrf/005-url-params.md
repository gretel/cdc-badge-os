---
title: "[LOW] Web Flasher lacks URL parameter parsing for deep linking"
severity: LOW
domain: web-flasher
lens: xss-csrf
labels:
  - web-flasher
  - url-parameters
  - user-experience
---

## Summary
The web-flasher `index.html` does not parse URL parameters (query string or hash). This means:
1. Users cannot share direct links to specific states (e.g., `?auto-connect=true`)
2. No potential for URL-based XSS since parameters are not read/rendered
3. Limited flexibility for integration with other tools

Since no URL parameters are read and rendered, there is **no direct XSS risk**, but this is a missed opportunity for features and could be implemented securely.

**File:** `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html`  
**Location:** Line 291-307 (JavaScript section)

## Impact
- **Minimal Security Risk:** Since no parameters are read, there's no XSS vulnerability
- **Feature Limitation:** Cannot implement features like:
  - Deep linking to specific firmware versions
  - Auto-connect on page load
  - Pre-filled parameters for automation
- **User Experience:** Users must manually navigate each time

## Evidence
The JavaScript section (lines 291-307) only fetches version info from GitHub API:
```javascript
async function fetchLatestVersion() {
  const badge = document.getElementById("version-badge");
  try {
    const resp = await fetch(
      "https://api.github.com/repos/krim404/cdc-badge-os/releases/latest"
    );
    // ...
  }
}
fetchLatestVersion();
```

No URL parameter parsing is performed. For comparison, a vulnerable pattern would be:
```javascript
// VULNERABLE (not present in current code):
const params = new URLSearchParams(window.location.search);
const version = params.get('version');
document.getElementById('version-badge').innerHTML = version;  // XSS risk!
```

## Recommended Fix
If URL parameter support is desired, implement it safely:

```javascript
// Add to the existing script section (after line 306)
function getUrlParams() {
  const params = new URLSearchParams(window.location.search);
  const hashParams = new URLSearchParams(window.location.hash.slice(1));
  return { ...Object.fromEntries(params), ...Object.fromEntries(hashParams) };
}

// Example usage (safe - uses textContent, not innerHTML):
const params = getUrlParams();
if (params.version) {
  const badge = document.getElementById("version-badge");
  badge.textContent = params.version;  // Safe: textContent auto-escapes
}
```

**Key Security Principles:**
1. Always use `textContent` or `setAttribute()`, never `innerHTML` for user input
2. Validate/sanitize parameters before use
3. Consider using `DOMParser` for any HTML rendering needs

## References
- [URLSearchParams API MDN](https://developer.mozilla.org/en-US/docs/Web/API/URLSearchParams)
- [OWASP XSS Prevention](https://cheatsheetseries.owasp.org/cheatsheets/Cross_SiteScripting_Prevention_Cheat_Sheet.html)
- [Safe URL Parameter Parsing](https://developer.mozilla.org/en-US/docs/Web/API/URLSearchParams)

</content>