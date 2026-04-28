---
title: "[LOW] Image lacks loading=lazy for below-fold content"
severity: LOW
domain: web-flasher
lens: adaptive-content
labels:
  - "performance"
  - "lazy-loading"
---

## Summary
The `badge.jpg` image in `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html` (line 222) loads eagerly without `loading="lazy"`. While the image is near the top of the page, adding lazy loading would still benefit users on slower connections by deferring the load until the image is closer to the viewport.

**Evidence:**
```html
<img src="badge.jpg" alt="CDC Badge v1.0" style="width: 80px; height: auto; border-radius: 0.5rem; flex-shrink: 0;">
```

No `loading` attribute specified (defaults to `eager`).

## Impact
- **Performance**: Image loads immediately even if user scrolls quickly to the flash button
- **Mobile data**: Slight waste on mobile where bandwidth is more constrained
- **LCP impact**: May affect Largest Contentful Paint if the image is considered the LCP element

## Evidence
- File: `web-flasher/index.html`
- Line: 222
- The image is in the second card, below the header with version badge

## Recommended Fix
Add `loading="lazy"` to the image:

```html
<img
  src="badge.jpg"
  alt="CDC Badge v1.0"
  loading="lazy"
  style="width: 80px; height: auto; border-radius: 0.5rem; flex-shrink: 0;"
/>
```

Note: If the image is above-the-fold on mobile (likely), consider keeping `loading="eager"` and instead ensure it has a `srcset` for optimal sizing (see issue #001).

## References
- [MDN: Loading attribute](https://developer.mozilla.org/en-US/docs/Web/Performance/Optimizing_page_loading#lazy_loading)
- [Web.dev: Lazy load images](https://web.dev/browser-level-image-lazy-loading/)
