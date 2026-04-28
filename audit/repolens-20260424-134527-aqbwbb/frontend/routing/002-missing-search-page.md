---
title: "[MEDIUM] Missing search.php for Doxygen Documentation Navigation"
severity: MEDIUM
domain: frontend
lens: routing
labels:
  - "audit:frontend/routing"
---

## Summary
The Doxygen-generated documentation references `search.php` in the navigation menu initialization, but this file does not exist. This breaks the server-side search functionality.

**Location:** `/input/20260423-132359-oj8ayc/cdc-badge-os/doxygen_output/html/index.html` and all HTML pages

```javascript
initMenu('',true,false,'search.php','Search',true);
```

## Impact
- **Search feature broken**: Users clicking the search box may encounter 404 errors
- **Incomplete documentation UX**: The search functionality is a key navigation feature in large documentation sites
- The JavaScript initializes with `serverSide=true`, expecting a PHP backend that doesn't exist

## Evidence
1. Multiple HTML files contain: `initMenu('',true,false,'search.php','Search',true)`
2. The `search.php` file does not exist in the doxygen_output/html directory
3. Client-side search files exist (`search/search.js`, `search/*.js`) but configuration points to server-side

From `index.html`:
```html
<script type="text/javascript">
var searchBox = new SearchBox("searchBox", "search/",'.html');
</script>
```

From `menu.js` (line ~50):
```javascript
if (serverSide) {
  searchBoxHtml='<div id="MSearchBox"...action="'+relPath+searchPage...
```

## Recommended Fix
**Option 1 (Recommended):** Configure Doxygen for client-side search only by editing the `Doxyfile`:

```
SEARCHENGINE = YES
SERVER_BASED_SEARCH = NO
EXTERNAL_SEARCH = NO
```

Then regenerate the documentation with `doxygen Doxyfile`.

**Option 2:** If server-side search is required, create a simple `search.php` that processes the search query and returns results from the existing search data files.

## References
- [Doxygen Search Configuration](https://www.doxygen.nl/manual/config.html#cfg_searchengine)
- [Doxygen Server-based Search](https://www.doxygen.nl/manual/config.html#cfg_server_based_search)
