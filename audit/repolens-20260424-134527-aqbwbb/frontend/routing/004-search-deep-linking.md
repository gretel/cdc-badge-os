---
title: "[MEDIUM] Search Results Not Deep-Linkable via URL Parameters"
severity: MEDIUM
domain: frontend
lens: routing
labels:
  - "audit:frontend/routing"
---

## Summary
While the Doxygen search JavaScript reads URL query parameters (`window.location.search`), there's no mechanism to:
1. Update the URL when a user performs a search
2. Share search results via a direct URL
3. Bookmark search results for later access

**Location:** `/input/20260423-132359-oj8ayc/cdc-badge-os/doxygen_output/html/search/search.js`

```javascript
this.Search = function(search) {
  if (!search) { // get search word from URL
    search = window.location.search;
    search = search.substring(1);  // Remove the leading '?'
    search = unescape(search);
  }
  // ... search logic
};
```

## Impact
- **Poor UX for sharing**: Users cannot share search results via URL
- **No bookmarking**: Users cannot bookmark specific search queries
- **Inconsistent behavior**: The search reads from URL but never writes to it
- **SEO impact**: Search results pages are not indexable by search engines

## Evidence
1. `search.js` reads `window.location.search` but never updates it
2. No `history.pushState()` or `window.location.hash` updates after search
3. No URL parameter encoding/decoding for search terms with special characters
4. The search function accepts a parameter but only uses URL when parameter is empty

From `search.js`:
```javascript
this.Search = function(search) {
  if (!search) { // get search word from URL
    search = window.location.search;
    search = search.substring(1);  // Remove the leading '?'
    search = unescape(search);
  }
  // ... processes search but never updates URL
};
```

## Recommended Fix
Add URL state management to the search functionality:

1. **Update URL on search** - After performing a search, update the URL:
```javascript
// After search results are displayed
const searchTerm = search.trim();
const newUrl = `${relPath}?query=${encodeURIComponent(searchTerm)}`;
history.pushState({search: searchTerm}, '', newUrl);
```

2. **Handle browser back/forward** - Add popstate listener:
```javascript
window.addEventListener('popstate', function(e) {
  if (e.state && e.state.search) {
    searchBox.DOMSearchField().value = e.state.search;
    searchBox.Search();
  }
});
```

3. **Proper URL encoding** - Use `encodeURIComponent()` instead of `unescape()`:
```javascript
search = decodeURIComponent(search);
```

## References
- [MDN: History API](https://developer.mozilla.org/en-US/docs/Web/API/History_API)
- [MDN: URLSearchParams](https://developer.mozilla.org/en-US/docs/Web/API/URLSearchParams)
- [Doxygen Search Implementation](https://www.doxygen.nl/manual/advanced.html#searchengine)
