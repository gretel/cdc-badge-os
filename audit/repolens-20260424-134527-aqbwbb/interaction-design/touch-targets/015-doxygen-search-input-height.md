---
title: "[LOW] Doxygen search input field height not optimized for touch"
severity: LOW
domain: documentation
lens: touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The search input field in Doxygen (`doxygen_output/html/search/search.css`) has a fixed height of 22px, which is below the recommended 44px minimum for touch targets. While there is a `@media(hover: none)` query that increases font size for iOS, it does not increase the overall touch target height.

**Location:** `doxygen_output/html/search/search.css:40-57`

## Impact
- **Mobile Usability:** 22px height is approximately half the recommended 44px minimum touch target
- **Accessibility:** Does not meet WCAG 2.5.8 Target Size (Minimum) guidelines
- **User Experience:** Small input area may be difficult to tap and focus on touch devices

## Evidence
The search field styles in `search.css` (lines 40-57):

```css
#MSearchField {
    display: inline-block;
    vertical-align: top;
    width: 7.5em;
    height: 22px;           /* Only 22px height */
    margin: 0 0 0 0.15em;
    padding: 0;
    line-height: 1em;
    border:none;
    color: var(--search-foreground-color);
    outline: none;
    font-family: var(--font-family-search);
    -webkit-border-radius: 0px;
    border-radius: 0px;
    background: none;
}

@media(hover: none) {
    /* to avoid zooming on iOS */
    #MSearchField {
        font-size: 16px;    /* Only increases font, not height */
    }
}
```

The search box container (`#MSearchBox .right`) has `height: 1.6em` (approximately 18-20px), and the search field itself is only 22px tall. For a typical 14-16px font, this creates a very narrow vertical touch target.

## Recommended Fix
Increase the search field height and add touch-specific sizing:

```css
#MSearchField {
    display: inline-block;
    vertical-align: top;
    width: 7.5em;
    height: 32px;           /* Increased from 22px */
    margin: 0 0 0 0.15em;
    padding: 6px 8px;       /* Add padding for better touch */
    line-height: 1.4em;
    border:none;
    color: var(--search-foreground-color);
    outline: none;
    font-family: var(--font-family-search);
    -webkit-border-radius: 0px;
    border-radius: 0px;
    background: none;
    min-height: 32px;       /* Minimum height for touch */
}

@media(hover: none) {
    /* to avoid zooming on iOS */
    #MSearchField {
        font-size: 16px;
    }
}

#MSearchBox .right {
    display: inline-block;
    vertical-align: middle;
    width: 1.4em;
    height: 32px;           /* Match search field height */
}

/* For coarse pointers, increase further */
@media (any-pointer: coarse) {
    #MSearchField {
        height: 44px;
        min-height: 44px;
        padding: 10px 12px;
    }
    
    #MSearchBox .right {
        height: 44px;
    }
}
```

## References
- [WCAG 2.5.8 Target Size (Minimum)](https://www.w3.org/WAI/WCAG21/Understanding/target-size.html)
- [Material Design Touch Targets](https://m3.material.io/foundations/interactions/touch-interaction/touch-targets)
- [MDN: CSS Media Queries - Pointer](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/any-pointer)
