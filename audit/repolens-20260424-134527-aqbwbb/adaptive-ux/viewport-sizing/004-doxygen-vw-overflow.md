---
title: "[LOW] Doxygen documentation uses 100vw in calc() causing horizontal overflow"
severity: LOW
domain: adaptive-ux/viewport-sizing
lens: viewport-sizing
labels:
  - "audit:adaptive-ux/viewport-sizing"
---

## Summary
The libtropic Doxygen documentation (`third_party/libtropic/docs/doxygen/html/doxygen-awesome.css`) uses `calc(100vw - ...)` expressions for mobile layouts (lines 749, 757, 1970, 1984). This can cause horizontal overflow on devices with scrollbars or narrow viewports.

**File:** `third_party/libtropic/docs/doxygen/html/doxygen-awesome.css`  
**Lines:** 749, 757, 1970, 1984

```css
/* Line 749 */
width: calc(100vw - 30px);

/* Line 757 */
width: calc(100vw - 110px);

/* Lines 1970, 1984 */
width: calc(100vw - 2 * var(--spacing-large));
```

## Impact
- `100vw` includes scrollbar width, causing horizontal overflow
- On mobile devices with vertical scrollbars, content may extend beyond viewport
- Horizontal scrollbars may appear unexpectedly
- Search box and member declaration tables can overflow horizontally

## Evidence
From `third_party/libtropic/docs/doxygen/html/doxygen-awesome.css`:

**Search box on mobile (lines 746-758):**
```css
@media screen and (max-width: 767px) {
    #MSearchBox {
        margin-top: var(--spacing-medium);
        margin-bottom: var(--spacing-medium);
        width: calc(100vw - 30px);  /* Line 749 */
    }

    #MSearchField {
        width: calc(100vw - 110px);  /* Line 757 */
    }
```

**Member declaration tables (lines 1968-1986):**
```css
@media screen and (max-width: 767px) {
    table.memberdecls tr[class^='memitem']:not(.inherit) {
        display: block;
        width: calc(100vw - 2 * var(--spacing-large));  /* Line 1970 */
    }

    table.memberdecls tr[style="display: table-row;"] {
        display: block !important;
        visibility: visible;
        width: calc(100vw - 2 * var(--spacing-large));  /* Line 1984 */
        animation: fade .5s;
    }
```

## Recommended Fix
Replace `100vw` with `100%` width on parent containers or use `calc(100% - ...)` instead:

**Option 1: Use container-relative widths**
```css
@media screen and (max-width: 767px) {
    #MSearchBox {
        margin-top: var(--spacing-medium);
        margin-bottom: var(--spacing-medium);
        width: calc(100% - 30px);  /* Use container width, not viewport */
    }

    #MSearchField {
        width: calc(100% - 110px);
    }
```

**Option 2: Use max-width with overflow handling**
```css
@media screen and (max-width: 767px) {
    table.memberdecls tr[class^='memitem']:not(.inherit) {
        display: block;
        width: 100%;
        max-width: calc(100vw - 2 * var(--spacing-large));
        overflow-x: auto;
    }
```

## References
- [CSS Working Group: Viewport Units](https://drafts.csswg.org/css-values-4/#viewport-relative-lengths)
- [MDN: vw unit](https://developer.mozilla.org/en-US/docs/Web/CSS/length/vw)
- [Horizontal Overflow with 100vw](https://css-tricks.com/the-100vw-unit/)
