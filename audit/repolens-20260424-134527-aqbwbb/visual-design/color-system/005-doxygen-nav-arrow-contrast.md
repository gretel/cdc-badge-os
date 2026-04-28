---
title: "[LOW] Navigation tree arrow color has low contrast against background"
severity: LOW
domain: visual-design
lens: color-system
labels:
  - "audit:visual-design/color-system"
---

## Summary
The doxygen documentation navigation tree uses `var(--nav-background-color)` for the arrow color (line 1510), which results in very low contrast. In light mode, this is `#F9FAFC` (off-white) on a similar background, making the arrows barely visible.

**File:** `doxygen_output/html/doxygen.css` (line 1510)

## Impact
- **Accessibility:** Users with visual impairments may not see the expand/collapse indicators
- **Usability:** The navigation tree structure is harder to parse when arrows are subtle
- **WCAG compliance:** The contrast ratio is well below the 3:1 requirement for UI components

## Evidence
**Line 1510:**
```css
.arrow {
  color: var(--nav-background-color);
  ...
}
```

**Line 71 (light mode):**
```css
--nav-background-color: #F9FAFC;
```

**Contrast calculation:**
- Arrow color: `#F9FAFC`
- Background: `#F9FAFC` (same) or `#FFFFFF` (body background)
- Contrast ratio: **1:1** (same color) or **1.05:1** (nearly identical)

This means the arrows are essentially invisible in light mode.

**Dark mode (line 268):**
```css
--nav-background-color: #101826;
```
The contrast is better but still low against the dark background.

## Recommended Fix
Define a dedicated arrow color variable with sufficient contrast:

**Add to line ~100 (after other nav colors):**
```css
--nav-arrow-color: #728DC1;  /* Better contrast, matches icon theme */
--nav-arrow-selected-color: #5373B4;
```

**Update line 1510:**
```css
.arrow {
  color: var(--nav-arrow-color);
  ...
}
```

For better accessibility, ensure the contrast ratio meets at least 3:1:
```css
--nav-arrow-color: #4665A2;  /* Contrast ratio: ~4.5:1 on light background */
```

This ensures the navigation tree arrows are clearly visible to all users.

## References
- [WCAG 2.1 - Contrast (Minimum) - UI Components](https://www.w3.org/WAI/WCAG21/Understanding/contrast-minimum.html)
- [MDN - CSS Custom Properties](https://developer.mozilla.org/en-US/docs/Web/CSS/--*)
