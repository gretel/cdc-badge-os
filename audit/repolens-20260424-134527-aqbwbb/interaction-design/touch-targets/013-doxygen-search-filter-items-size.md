---
title: "[LOW] Doxygen search filter dropdown items have minimal vertical padding"
severity: LOW
domain: documentation
lens: touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The search filter dropdown items in Doxygen (`.SelectItem` and `a.SelectItem` in `doxygen_output/html/search/search.css`) have minimal vertical padding, relying only on font size (8pt) without explicit line-height or padding. This creates undersized touch targets for the filter selection menu.

**Location:** `doxygen_output/html/search/search.css:196-217`

## Impact
- **Mobile Usability:** Filter dropdown items with 8pt font and no vertical padding create touch targets of approximately 12-15px height
- **Accessibility:** Does not meet WCAG 2.5.8 Target Size (Minimum) guidelines
- **User Experience:** Small tap areas in the search filter dropdown may lead to selection errors on touch devices

## Evidence
The search filter item styles in `search.css` (lines 196-217):

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
```

The `.SelectItem` class uses 8pt font (≈11px) with only horizontal padding (2px left, 12px right) and no vertical padding. The `a.SelectItem` link has the same issue - horizontal padding is specified but vertical padding relies solely on the font's natural line-height, resulting in touch targets of approximately 12-15px height.

## Recommended Fix
Add explicit vertical padding and minimum height to filter items:

```css
.SelectItem {
    font: 8pt var(--font-family-search);
    padding: 8px 12px; /* Changed from padding-left: 2px; padding-right: 12px */
    border: 0px;
    display: block;
    min-height: 24px;
    line-height: 1.4;
}

span.SelectionMark {
    margin-right: 4px;
    font-family: var(--font-family-monospace);
    outline-style: none;
    text-decoration: none;
    vertical-align: middle;
}

a.SelectItem {
    display: block;
    outline-style: none;
    color: var(--search-filter-foreground-color);
    text-decoration: none;
    padding: 8px 12px; /* Changed from padding-left: 6px; padding-right: 12px */
    min-height: 24px;
    line-height: 1.4;
}

a.SelectItem:focus,
a.SelectItem:active {
    color: var(--search-filter-foreground-color);
    outline-style: none;
    text-decoration: none;
}

a.SelectItem:hover {
    color: var(--search-filter-highlight-text-color);
    background-color: var(--search-filter-highlight-bg-color);
    outline-style: none;
    text-decoration: none;
    cursor: pointer;
    display: block;
}

/* For coarse pointers, increase further */
@media (any-pointer: coarse) {
    .SelectItem,
    a.SelectItem {
        padding: 12px 14px;
        min-height: 36px;
    }
}
```

## References
- [WCAG 2.5.8 Target Size (Minimum)](https://www.w3.org/WAI/WCAG21/Understanding/target-size.html)
- [Material Design Touch Targets](https://m3.material.io/foundations/interactions/touch-interaction/touch-targets)
- [MDN: CSS Media Queries - Pointer](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/any-pointer)
