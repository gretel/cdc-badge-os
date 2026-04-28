---
title: "[INFO] GitHub API Call Without Rate Limit Handling"
severity: LOW
domain: security-headers
lens: security-headers
labels:
  - "audit:security/security-headers"
---

## Summary
The web-flasher at `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html` calls the GitHub Releases API to fetch the latest version but doesn't handle rate limiting or API failures gracefully.

## Impact
- When the GitHub API rate limit is exceeded (60/hour for unauthenticated), the version badge shows "Version info unavailable"
- No caching of the version data means repeated page loads hammer the API
- Users may see outdated version information

## Evidence
File: `web-flasher/index.html`
- Line 281-298: GitHub API fetch without rate limit handling:
  ```javascript
  async function fetchLatestVersion() {
    const badge = document.getElementById("version-badge");
    try {
      const resp = await fetch(
        "https://api.github.com/repos/krim404/cdc-badge-os/releases/latest"
      );
      if (!resp.ok) throw new Error("API request failed");
      const data = await resp.json();
      badge.textContent = data.tag_name;
      badge.classList.remove("loading");
    } catch {
      badge.textContent = "Version info unavailable";
    }
  }
  ```

## Recommended Fix
Add caching and better error handling:

```javascript
async function fetchLatestVersion() {
  const badge = document.getElementById("version-badge");
  const CACHE_KEY = 'cdc-badge-version';
  const CACHE_DURATION = 3600000; // 1 hour
  
  try {
    // Check cache first
    const cached = JSON.parse(localStorage.getItem(CACHE_KEY) || '{}');
    if (Date.now() - cached.timestamp < CACHE_DURATION) {
      badge.textContent = cached.version;
      badge.classList.remove("loading");
      return;
    }
    
    const resp = await fetch(
      "https://api.github.com/repos/krim404/cdc-badge-os/releases/latest",
      { headers: { 'Accept': 'application/vnd.github.v3+json' } }
    );
    
    if (!resp.ok) throw new Error("API request failed");
    const data = await resp.json();
    
    // Cache the result
    localStorage.setItem(CACHE_KEY, JSON.stringify({
      version: data.tag_name,
      timestamp: Date.now()
    }));
    
    badge.textContent = data.tag_name;
    badge.classList.remove("loading");
  } catch (e) {
    // Show cached version if available
    const cached = JSON.parse(localStorage.getItem(CACHE_KEY) || '{}');
    if (cached.version) {
      badge.textContent = cached.version + " (cached)";
    } else {
      badge.textContent = "v0.5.0"; // Fallback to known version
    }
    badge.classList.remove("loading");
  }
}
```

## References
- [GitHub API Rate Limits](https://docs.github.com/en/rest/overview/resources-in-the-rest-api#rate-limits)
- [LocalStorage for Caching](https://developer.mozilla.org/en-US/docs/Web/API/Window/localStorage)
