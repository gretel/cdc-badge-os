---
title: "[LOW] GitHub API error handling lacks retry logic and rate limit awareness"
severity: LOW
domain: api-design
lens: rest-conventions
labels:
  - "audit:api-design/rest-conventions"
---

## Summary
In `web-flasher/index.html` (lines 285-298), the `fetchLatestVersion()` function calls the GitHub REST API (`https://api.github.com/repos/krim404/cdc-badge-os/releases/latest`) with basic error handling but lacks:
1. Rate limit awareness (GitHub API has 60 requests/hour unauthenticated)
2. Retry logic for transient failures
3. Proper HTTP status code handling beyond `resp.ok`

## Impact
- **User Experience**: During rate limiting (403 with `X-RateLimit-Remaining: 0`), users see generic "Version info unavailable" instead of helpful feedback
- **Reliability**: Transient network errors or 5xx responses from GitHub are not retried
- **Maintainability**: Code does not follow REST client best practices for API consumption

## Evidence
```javascript
// web-flasher/index.html:285-298
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

Current issues:
- No check for `403` rate limit status with `X-RateLimit-Reset` header
- No distinction between client errors (4xx) and server errors (5xx)
- No retry mechanism for transient failures
- No timeout configuration (defaults can be slow)

## Recommended Fix
Add proper REST client error handling:

```javascript
async function fetchLatestVersion() {
  const badge = document.getElementById("version-badge");
  const maxRetries = 3;
  
  for (let attempt = 0; attempt < maxRetries; attempt++) {
    try {
      const resp = await fetch(
        "https://api.github.com/repos/krim404/cdc-badge-os/releases/latest",
        { timeout: 5000 }
      );
      
      // Handle rate limiting
      if (resp.status === 403) {
        const resetTime = resp.headers.get('X-RateLimit-Reset');
        if (resetTime) {
          const waitMin = Math.ceil((parseInt(resetTime) - Date.now()/1000) / 60);
          badge.textContent = `Rate limited. Try in ~${waitMin} min`;
          return;
        }
      }
      
      if (!resp.ok) {
        // Retry only on 5xx (server errors)
        if (resp.status >= 500 && attempt < maxRetries - 1) {
          await new Promise(r => setTimeout(r, 1000 * (attempt + 1)));
          continue;
        }
        throw new Error(`HTTP ${resp.status}`);
      }
      
      const data = await resp.json();
      badge.textContent = data.tag_name;
      badge.classList.remove("loading");
      return;
    } catch (err) {
      if (attempt === maxRetries - 1) {
        badge.textContent = "Version info unavailable";
      } else {
        await new Promise(r => setTimeout(r, 1000 * (attempt + 1)));
      }
    }
  }
}
```

## References
- [GitHub REST API Rate Limits](https://docs.github.com/en/rest/overview/resources-in-the-rest-api#rate-limiting)
- [RFC 6585 - Additional HTTP Status Codes](https://tools.ietf.org/html/rfc6585) (429 Too Many Requests)
- [HTTP Status Codes for REST APIs](https://developer.mozilla.org/en-US/docs/Web/HTTP/Status)
