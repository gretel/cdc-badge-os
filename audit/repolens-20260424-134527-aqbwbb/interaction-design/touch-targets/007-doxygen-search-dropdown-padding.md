---
title: "[LOW] Doxygen search dropdown items have minimal padding"
severity: LOW
domain: documentation
lens: touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The search filter dropdown items (`doxygen_output/html/search/search.css`) have minimal vertical padding, resulting in touch targets that may be undersized for comfortable tapping on mobile devices.

**Location:** `doxygen_output/html/search/search.css:210-233`

## Impact
- **Mobile Usability:** Search dropdown items may have less than 44px height when considering the font size (8pt ≈ 11px) and padding (2px left, 12px right), making them harder to tap accurately
- **Accessibility:** May not meet WCAG 2.5.8 Target Size (Minimum) guidelines for touch interfaces
- **User Experience:** Dense list of search options increases likelihood of accidental taps on mobile

## Evidence
The search dropdown item styles in `search/search.css` (lines 210-233):

```css
.SelectItem {
    font: 8pt var(--font-family-search);
    padding-left:  2px;
    padding-right: 12px;
    border: 0px;
}

a.SelectItem {
    display: block;
    outline-style: none;
    color: var(--search-filter-foreground-color);
    text-decoration: none;
    padding-left:   6px;
    padding-right: 12px;
}

a.SelectItem:hover {
    color: var(--search-filter-highlight-text-color);
    background-color: var(--search-filter-highlight-bg-color);
    outline-style: none;
    text-decoration: none;
    cursor: pointer;
    display: block;
}
```

The `.SelectItem` uses 8pt font (≈11px) with only 2px left padding and 12px right padding. No explicit height or vertical padding is defined, meaning the total touch height is approximately 11-15px, well below the 44px recommendation.

## Recommended Fix
Add explicit minimum height and vertical padding to ensure adequate touch targets:

```css
.SelectItem {
    font: 8pt var(--font-family-search);
    padding-left:  2px;
    padding-right: 12px;
    padding-top: 8px;
    padding-bottom: 8px;
    min-height: 44px; /* Ensure minimum touch target height */
    border: 0px;
}

a.SelectItem {
    display: block;
    outline-style: none;
    color: var(--search-filter-foreground-color);
    text-decoration: none;
    padding-left:   6px;
    padding-right: 12px;
    padding-top: 8px;
    padding-bottom: 8px;
    min-height: 44px; /* Ensure minimum touch target height */
}

a.SelectItem:hover {
    color: var(--search-filter-highlight-text-color);
    background-color: var(--search-filter-highlight-bg-color);
    outline-style: none;
    text-decoration: none;
    cursor: pointer;
    display: block;
}

/* Optionally increase for coarse pointers */
@media (any-pointer: coarse) {
    .SelectItem {
        min-height: 48px;
        padding-top: 10px;
        padding-bottom: 10px;
    }
    a.SelectItem {
        min-height: 48px;
        padding-top: 10px;
        padding-bottom: 10px;
    }
}
```

## References
- [WCAG 2.5.8 Target Size (Minimum)](https://www.w3.org/WAI/WCAG21/Understanding/target-size.html)
- [Material Design Touch Targets](https://m3.material.io/foundations/interactions/touch-interaction/touch-targets)
- [MDN: CSS Media Queries - Pointer](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/any-pointer)
