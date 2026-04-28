---
title: "[MEDIUM] Fixed-width navigation sidebar may overflow on small screens"
severity: MEDIUM
domain: frontend
lens: responsive-design
labels:
  - responsive-design
  - layout
  - css
---

## Summary
The doxygen documentation navigation sidebar uses a fixed width of 250px (`navtree.css:245`) without responsive adjustments for smaller screens. The sidebar also uses `position: absolute` positioning which can cause overflow issues on narrow viewports.

**Location**: `doxygen_output/html/navtree.css:245`

```css
#page-nav {
  background: var(--nav-background-color);
  display: block;
  width: 250px;
  box-sizing: content-box;
  position: relative;
  border-left: 1px solid var(--nav-border-color);
}
```

Additionally, the navigation tree container has `white-space: nowrap` (`navtree.css:13`) which prevents text wrapping and can cause horizontal overflow.

## Impact
**Moderate usability impact** on mobile and small tablet screens:
- On screens narrower than ~320px (small phones in portrait), the 250px sidebar leaves minimal space for content
- Long navigation item names may overflow or be truncated due to `white-space: nowrap`
- Fixed positioning doesn't adapt to different screen sizes
- Horizontal scrollbar may appear on small screens

## Evidence
**Key problematic CSS rules**:

1. Fixed sidebar width (line 245):
```css
#page-nav {
  width: 250px;
}
```

2. No whitespace wrapping (line 13):
```css
#nav-tree li {
  white-space:nowrap;
}
```

3. No media queries for mobile adaptation (only `@media print` at line 229)

## Recommended Fix
Add responsive media queries to adapt the navigation sidebar for smaller screens:

```css
/* Add mobile breakpoint */
@media screen and (max-width: 767px) {
  #page-nav {
    width: 200px; /* Reduce width on smaller screens */
  }
  
  #nav-tree li {
    white-space: normal; /* Allow text wrapping */
    word-break: break-word; /* Break long words if needed */
  }
}

@media screen and (max-width: 480px) {
  #page-nav {
    width: 100%; /* Full width on very small screens */
    position: relative; /* Remove fixed positioning */
  }
  
  #nav-tree {
    font-size: 12px; /* Smaller font for mobile */
  }
}
```

Alternatively, use a max-width approach:
```css
#page-nav {
  width: 250px;
  max-width: 80vw; /* Don't exceed 80% of viewport width */
}
```

## References
- [MDN - Media Queries](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_media_queries/Using_media_queries)
- [CSS Tricks - Responsive Navigation](https://css-tricks.com/responsive-navigation-the-complete-guide/)
- [Web.dev - Build responsive layouts](https://web.dev/responsive-web-design-basics/)
