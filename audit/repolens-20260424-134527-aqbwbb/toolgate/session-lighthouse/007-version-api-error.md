---
title: "[MEDIUM] GitHub API call lacks error handling for version display"
severity: MEDIUM
domain: cdc-badge-os
lens: session-lighthouse
labels:
  - "audit:toolgate/session-lighthouse"
---

## Summary
The GitHub API call to fetch the latest version (lines 291-306) has basic error handling but doesn't provide user feedback on failure beyond changing the badge text.

**File:** `web-flasher/index.html` (lines 291-306)

## Impact
- **User experience**: Users may not understand why version badge shows "unavailable"
- **Lighthouse score**: May flag console errors if API fails
- **Reliability**: GitHub API rate limits could cause failures

## Evidence
Current implementation (lines 291-306):
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
fetchLatestVersion();
```

Issues:
1. No retry logic for transient failures
2. No distinction between rate limit vs. network error
3. No console logging for debugging
4. API could be rate-limited (60/hour for unauthenticated)

## Recommended Fix
Improve error handling:

```javascript
async function fetchLatestVersion() {
  const badge = document.getElementById("version-badge");
  try {
    const resp = await fetch(
      "https://api.github.com/repos/krim404/cdc-badge-os/releases/latest",
      { headers: { "Accept": "application/vnd.github.v3+json" } }
    );
    
    if (!resp.ok) {
      const error = await resp.json().catch(() => ({}));
      console.warn("Failed to fetch version:", error.message || resp.statusText);
      
      if (resp.status === 403) {
        badge.textContent = "Rate limited";
        badge.title = "GitHub API rate limit reached";
      } else {
        badge.textContent = "v1.0"; // Fallback to hardcoded version
        badge.title = "Latest version from GitHub";
      }
      return;
    }
    
    const data = await resp.json();
    badge.textContent = data.tag_name;
    badge.classList.remove("loading");
  } catch (error) {
    console.warn("Version fetch error:", error);
    badge.textContent = "v1.0"; // Fallback
  }
}
```

**Alternative:** Hardcode version in HTML to eliminate API dependency:
```html
<span class="version-badge">v1.0.0</span>
```

## References
- [GitHub API Rate Limiting](https://docs.github.com/en/rest/overview/resources-in-the-rest-api#rate-limiting)
- [MDN fetch API](https://developer.mozilla.org/en-US/docs/Web/API/Fetch_API)
