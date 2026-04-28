---
title: "[LOW] Missing meta viewport description attribute"
severity: LOW
domain: cdc-badge-os
lens: session-lighthouse
labels:
  - "audit:toolgate/session-lighthouse"
---

## Summary
The viewport meta tag is present but could include a `description` attribute for better accessibility documentation (minor improvement).

**File:** `web-flasher/index.html` (line 5)

## Impact
- **Minor SEO benefit**: Some search engines use viewport description
- **Accessibility**: Helps document responsive design intent

## Evidence
Current implementation (line 5):
```html
<meta name="viewport" content="width=device-width, initial-scale=1.0">
```

## Recommended Fix
Add description attribute:
```html
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<meta name="description" content="Responsive web flasher for CDC Badge. Works on desktop and mobile browsers with Web Serial support.">
```

Note: The main `description` meta tag is more important for SEO.

## References
- [MDN Viewport meta tag](https://developer.mozilla.org/en-US/docs/Web/HTML/Viewport_meta_tag)
