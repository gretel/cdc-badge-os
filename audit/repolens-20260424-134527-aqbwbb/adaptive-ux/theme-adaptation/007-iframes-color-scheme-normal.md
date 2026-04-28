---
title: "[LOW] Doxygen search results iframe uses `color-scheme: normal` preventing dark mode adaptation"
severity: LOW
domain: adaptive-ux/theme-adaptation
lens: theme-adaptation
labels:
  - dark-mode
  - documentation
---

## Summary
The Doxygen Awesome CSS (`third_party/libtropic/docs/doxygen/html/doxygen-awesome.css`) explicitly sets `color-scheme: normal` on all iframes (line 700), which prevents browser-native dark mode adaptation for the search results iframe. While a filter is applied to invert colors (lines 704-711), the `color-scheme: normal` property may interfere with proper rendering.

## Impact
- The search results iframe may not properly adapt to dark mode
- Browser-native controls within the iframe (scrollbars, form inputs) may remain in light mode
- The filter-based inversion may cause visual artifacts or performance issues

## Evidence
**File: `third_party/libtropic/docs/doxygen/html/doxygen-awesome.css`**

```css
/* Line 699-701 */
iframe {
    color-scheme: normal;  /* Forces light mode for all iframes */
}

/* Lines 703-711 */
@media (prefers-color-scheme: dark) {
    html:not(.light-mode) iframe#MSearchResults {
        filter: invert() hue-rotate(180deg);
    }
}

html.dark-mode iframe#MSearchResults {
    filter: invert() hue-rotate(180deg);
}
```

The `color-scheme: normal` forces all iframes to use light mode, but the filter tries to invert them for dark mode. This is a workaround that may not work consistently across browsers.

## Recommended Fix
Remove the `color-scheme: normal` override or make it conditional:

```css
/* Remove this line or make it conditional */
iframe {
    /* color-scheme: normal;  <-- Remove or comment out */
}

/* Or allow dark mode for search results iframe */
iframe#MSearchResults {
    color-scheme: normal dark;
}

@media (prefers-color-scheme: dark) {
    html:not(.light-mode) iframe#MSearchResults {
        /* Consider removing filter if color-scheme handles it */
        filter: invert() hue-rotate(180deg);
    }
}

html.dark-mode iframe#MSearchResults {
    filter: invert() hue-rotate(180deg);
}
```

Alternatively, consider using `color-scheme: light dark;` to allow the iframe to adapt to the parent's theme.

## References
- [MDN: color-scheme](https://developer.mozilla.org/en-US/docs/Web/CSS/color-scheme)
- [CSS-Tricks: color-scheme and Iframes](https://css-tricks.com/a-complete-guide-to-css-color-scheme/)
