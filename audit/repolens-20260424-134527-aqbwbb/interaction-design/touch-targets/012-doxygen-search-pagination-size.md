---
title: "[LOW] Doxygen search pagination links have minimal touch targets"
severity: LOW
domain: documentation
lens: touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The pagination links in Doxygen search results (`.pages` class in `doxygen_output/html/search/search.css`) have minimal sizing with only `line-height: 17px` and no explicit padding, resulting in touch targets that are undersized for comfortable tapping on mobile devices.

**Location:** `doxygen_output/html/search/search.css:360-364`

## Impact
- **Mobile Usability:** Pagination links with 17px line-height and no padding create very small touch targets of approximately 17px height
- **Accessibility:** Does not meet WCAG 2.5.8 Target Size (Minimum) guidelines (44x44px recommended)
- **User Experience:** Small clickable areas on pagination controls may lead to tap errors on touch devices

## Evidence
The pagination link styles in `search.css` (lines 360-364):

```css
.pages {
    line-height: 17px;
    margin-left: 4px;
    text-decoration: none;
}
```

The `.pages` class uses only `line-height: 17px` with no padding. For typical 8-10pt text, the total touch height is approximately 17px, well below the 44px recommendation. The adjacent `.pages b` (active page indicator) has slightly better padding (5px 5px 3px 5px) but still undersized.

Additionally, the `.SRPage .SRStatus` element (line 331-336) has only `padding: 2px 5px` and `font-size: 8pt`, creating another undersized touch target.

## Recommended Fix
Add explicit minimum dimensions and padding to pagination links:

```css
.pages {
    line-height: 17px;
    margin-left: 4px;
    text-decoration: none;
    display: inline-block;
    padding: 8px 10px; /* Add padding for touch target */
    min-height: 24px;  /* Minimum practical touch height */
}

.pages b {
   color: var(--nav-foreground-color);
   padding: 8px 10px; /* Increased from 5px 5px 3px 5px */
   background-color: var(--nav-menu-active-bg);
   border-radius: 4px;
   display: inline-block; /* Ensure consistent sizing */
   min-height: 24px;
}

/* For coarse pointers, increase further */
@media (any-pointer: coarse) {
    .pages {
        padding: 12px 14px;
        min-height: 36px;
    }
    
    .pages b {
        padding: 12px 14px;
        min-height: 36px;
    }
}

/* Also fix SRStatus */
.SRPage .SRStatus {
    padding: 8px 10px; /* Increased from 2px 5px */
    font-size: 8pt;
    font-style: italic;
    font-family: var(--font-family-search);
    display: inline-block;
    min-height: 24px;
}

@media (any-pointer: coarse) {
    .SRPage .SRStatus {
        padding: 12px 14px;
        min-height: 36px;
    }
}
```

## References
- [WCAG 2.5.8 Target Size (Minimum)](https://www.w3.org/WAI/WCAG21/Understanding/target-size.html)
- [Material Design Touch Targets](https://m3.material.io/foundations/interactions/touch-interaction/touch-targets)
- [MDN: CSS Media Queries - Pointer](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/any-pointer)
