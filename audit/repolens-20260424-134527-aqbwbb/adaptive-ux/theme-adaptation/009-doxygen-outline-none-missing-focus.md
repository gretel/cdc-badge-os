---
title: "[MEDIUM] Doxygen CSS removes focus outlines without alternative focus indicator"
severity: MEDIUM
domain: adaptive-ux/theme-adaptation
lens: theme-adaptation
labels:
  - accessibility
  - focus
---

## Summary
The doxygen documentation CSS files remove focus outlines but lack proper alternative focus indicators, particularly in high-contrast mode. Affected files:

1. **`doxygen_output/html/doxygen.css`** (line 1466): `outline:none;` on directory links
2. **`third_party/libtropic/docs/doxygen/html/doxygen-awesome.css`** (line 835): `outline: none;` on nav tree links

While the doxygen-awesome.css has some focus styles (line 539: `outline: auto;` for menu links), these are inconsistent and don't provide adequate visibility for keyboard users, especially in high-contrast mode.

## Impact
- Keyboard users may lose track of which element is focused
- Users with visual impairments relying on high-contrast mode may not see focus indicators
- Inconsistent focus styles between different navigation elements
- Potential WCAG 2.1 Level AA compliance gap (Criterion 2.4.7: Focus Visible)

## Evidence
**File: `doxygen_output/html/doxygen.css`**
```css
/* Line 1466 */
.directory td.entry a {
    outline:none;
}
```

**File: `third_party/libtropic/docs/doxygen/html/doxygen-awesome.css`**
```css
/* Line 835 */
#nav-tree .item > a:focus {
    outline: none;
}

/* Line 539 - inconsistent approach */
.sm-dox a:focus {
    outline: auto;
}
```

No `prefers-contrast: more` media query provides enhanced focus styles.

## Recommended Fix
1. **Replace `outline:none` with visible focus styles:**

```css
/* doxygen_output/html/doxygen.css */
.directory td.entry a {
    outline: 2px solid var(--link-color, #0032FF);
    outline-offset: 2px;
}

/* third_party/libtropic/docs/doxygen/html/doxygen-awesome.css */
#nav-tree .item > a:focus {
    outline: 2px solid var(--primary-color);
    outline-offset: -2px;
}
```

2. **Add high-contrast focus enhancement:**

```css
@media (prefers-contrast: more) {
    a:focus,
    .sm-dox a:focus,
    #nav-tree .item > a:focus,
    .directory td.entry a:focus {
        outline: 3px solid #fff;
        outline-offset: 2px;
    }
}
```

3. **Ensure consistency across all focusable elements** - use the same focus style pattern throughout.

## References
- [WCAG 2.1 Criterion 2.4.7: Focus Visible](https://www.w3.org/TR/WCAG21/#focus-visible)
- [MDN: outline CSS property](https://developer.mozilla.org/en-US/docs/Web/CSS/outline)
- [CSS-Tricks: Focus Styles](https://css-tricks.com/focus-styles/)

</content>