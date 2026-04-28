---
title: "[MEDIUM] Search results dropdown has fixed dimensions that may overflow on mobile"
severity: MEDIUM
domain: frontend
lens: responsive-design
labels:
  - responsive-design
  - layout
  - css
---

## Summary
The doxygen search results dropdown uses fixed dimensions (`width: 300px`, `height: 400px`) that don't adapt to smaller screens. On mobile devices with narrow viewports, this can cause horizontal overflow and require scrolling.

**Location**: `doxygen_output/html/search/search.css:265-266`

```css
#MSearchResultsWindow {
  display: none;
  position: absolute;
  left: auto;
  right: 4px;
  top: 0;
  border: 1px solid var(--search-results-border-color);
  background-color: var(--search-results-background-color);
  backdrop-filter: var(--search-results-backdrop-filter);
  -webkit-backdrop-filter: var(--search-results-backdrop-filter);
  z-index:10000;
  width: 300px;
  height: 400px;
  overflow: auto;
  border-radius: 8px;
  transform: translate(0, 20px);
}
```

## Impact
**Moderate usability impact** on mobile devices:
- On screens narrower than 320px, the 300px width leaves minimal margin
- Fixed height of 400px may exceed viewport height on small screens with browser chrome
- Search results may not be fully visible without scrolling
- Poor experience on portrait-oriented mobile devices

## Evidence
**Location**: `doxygen_output/html/search/search.css:254-272`

The search results window:
- Has `width: 300px` (line 265)
- Has `height: 400px` (line 266)
- Uses `position: absolute` with `right: 4px` (lines 258-260)
- No media queries to adjust dimensions for mobile (`search.css` only has hover media query at line 58)

For comparison, the doxygen-awesome CSS has proper responsive handling:
```css
/* doxygen-awesome.css line 749 */
@media screen and (max-width: 767px) {
    .search-box {
        width: calc(100vw - 30px);
    }
}
```

## Recommended Fix
Add responsive media queries for the search results window:

```css
/* Mobile-first responsive search results */
@media screen and (max-width: 767px) {
  #MSearchResultsWindow {
    width: calc(100vw - 20px); /* Fill most of viewport width */
    max-width: 300px; /* Cap at original width on larger screens */
    height: calc(50vh - 50px); /* Use viewport height with margin */
    max-height: 400px; /* Cap at original height */
    right: 10px;
    left: 10px; /* Center on smaller screens */
  }
}

@media screen and (max-width: 480px) {
  #MSearchResultsWindow {
    width: calc(100vw - 20px);
    height: calc(60vh - 50px); /* More height on small screens */
  }
}
```

Also update the search box itself for mobile:
```css
@media screen and (max-width: 767px) {
  #MSearchBox {
    width: 100%;
    max-width: 200px;
  }
  
  #MSearchField {
    width: 100px;
  }
}
```

## References
- [MDN - CSS Viewport Units](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_values_syntax/Viewport-percentage_units)
- [CSS Tricks - Responsive Modals/Dropdowns](https://css-tricks.com/responsive-modals/)
- [Web Accessibility - Responsive Design](https://www.w3.org/WAI/WCAG21/Understanding/reflow.html)
