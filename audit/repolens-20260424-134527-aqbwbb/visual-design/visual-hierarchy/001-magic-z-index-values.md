---
title: "[HIGH] Magic z-index values including MAX_INT (2147483647) without centralized scale"
severity: HIGH
domain: visual-design
lens: visual-hierarchy
labels:
  - "audit:visual-design/visual-hierarchy"
---

## Summary
The codebase contains scattered z-index values with no centralized scale or naming convention. Most critically, `z-index: 2147483647` (MAX_INT) is used in `doxygen_output/html/doxygen.css:2276` for the tooltip element. Additional magic numbers include:

- `doxygen_output/html/doxygen.css:555` - `z-index: 10` (for `#main-menu a:focus`)
- `doxygen_output/html/doxygen.css:1818` - `z-index: 100`
- `doxygen_output/html/doxygen.css:1869` - `z-index: 10`
- `doxygen_output/html/doxygen.css:2276` - `z-index: 2147483647` (tooltip)
- `doxygen_output/html/search/search.css:31` - `z-index: 102`
- `doxygen_output/html/search/search.css:190` - `z-index: 10001`
- `doxygen_output/html/search/search.css:264` - `z-index: 10000`
- `doxygen_output/html/navtree.css:128,147,265` - `z-index: 1`
- `third_party/libtropic/docs/doxygen/html/doxygen-awesome.css:624` - `z-index: 9999`
- `doxygen_output/html/tabs.css` - `z-index: 9999` (for `.sm` menu)

## Impact
1. **Unpredictable stacking**: Without a documented stacking context map, developers cannot reliably determine which layer sits above which.
2. **MAX_INT anti-pattern**: Using `2147483647` (MAX_INT) leaves no room for overlays above tooltips (e.g., modals, notifications).
3. **Competing values**: Multiple components (search results `10001`, dropdowns `9999`, tooltips `2147483647`) fight for top layer without clear hierarchy.
4. **Maintenance burden**: Future developers must guess which z-index to use, leading to more magic numbers.

## Evidence
```css
/* doxygen_output/html/doxygen.css:2276 */
#powerTip {
    position: absolute;
    z-index: 2147483647;  /* MAX_INT - leaves no room for higher overlays */
}

/* doxygen_output/html/search/search.css:190 */
#MSearchResultsWindow {
    z-index: 10001;  /* Competes with other high values */
}

/* third_party/libtropic/docs/doxygen/html/doxygen-awesome.css:624 */
#MSearchSelectWindow, #MSearchResultsWindow {
    z-index: 9999;  /* Different value for similar purpose */
}
```

## Recommended Fix
1. **Create a centralized z-index scale** in a CSS variables file or root `:root` selector:
```css
:root {
    --z-base: 0;
    --z-dropdown: 100;
    --z-sticky: 200;
    --z-tooltip: 300;
    --z-modal-backdrop: 400;
    --z-modal: 500;
    --z-popover: 600;
    --z-tooltip-max: 700;
}
```

2. **Replace magic numbers** with CSS variables:
```css
#powerTip {
    position: absolute;
    z-index: var(--z-tooltip);  /* Instead of 2147483647 */
}

#MSearchResultsWindow {
    z-index: var(--z-popover);  /* Instead of 10001 */
}
```

3. **Document the stacking context** in a README or style guide with a visual hierarchy map.

4. **Audit existing values** to ensure logical ordering (dropdowns < modals < tooltips).

## References
- [MDN: z-index](https://developer.mozilla.org/en-US/docs/Web/CSS/z-index)
- [CSS Tricks: Stacking Context](https://css-tricks.com/stacking-context/)
- [Design Tokens for z-index](https://designsystem.digital.gov/design-tokens/layer/)
