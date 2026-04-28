---
title: "[LOW] Image could benefit from lazy loading"
severity: LOW
domain: cdc-badge-os
lens: session-lighthouse
labels:
  - "audit:toolgate/session-lighthouse"
---

## Summary
The `badge.jpg` image on the web-flasher page could use `loading="lazy"` attribute, though this is a minor optimization since the image is near the top of the page.

**File:** `web-flasher/index.html` (line 222)

## Impact
- **Minor performance gain**: Reduces initial page weight if image is below fold
- **Lighthouse score**: May flag "uses-lazy-loading" depending on viewport
- **Note**: Since the image is in the first card near the top, lazy loading benefit is minimal

## Evidence
Current implementation (line 222):
```html
<img src="badge.jpg" alt="CDC Badge v1.0" style="width: 80px; height: auto; border-radius: 0.5rem; flex-shrink: 0;">
```

Missing: `loading="lazy"` attribute

## Recommended Fix
Add lazy loading attribute:

```html
<img src="badge.jpg" alt="CDC Badge v1.0" loading="lazy" style="width: 80px; height: auto; border-radius: 0.5rem; flex-shrink: 0;">
```

**Note:** For images "above the fold" (visible on initial load), lazy loading provides little benefit. Consider keeping it if the image is critical for above-fold content.

## References
- [Lighthouse: Image lazy loading](https://web.dev/uses-lazy-loading-images/)
- [MDN loading attribute](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/img#attributes)
