---
title: "[MEDIUM] Navigation breadcrumb separator has insufficient contrast in light and dark modes"
severity: MEDIUM
domain: visual-design
lens: color-system
labels:
  - "audit:visual-design/color-system"
---

## Summary
The doxygen documentation navigation breadcrumb separator color has very low contrast against the background in both light and dark modes, failing WCAG AA requirements for UI components (3:1).

**File:** `doxygen_output/html/doxygen.css` (lines 74, 271)

## Impact
- **Accessibility:** Users with visual impairments may not see the breadcrumb separators clearly
- **Usability:** Navigation structure is harder to parse when separators are subtle
- **WCAG AA compliance:** Both light and dark modes fail the 3:1 contrast requirement for UI components

## Evidence
**Light mode (line 74):**
```css
--nav-breadcrumb-separator-color: #C4CFE5;
```

**Dark mode (line 271):**
```css
--nav-breadcrumb-separator-color: #212F4B;
```

**Contrast calculations:**

| Mode | Separator | Background | Ratio | WCAG AA (3:1) |
|------|-----------|------------|-------|---------------|
| Light | #C4CFE5 | #FFFFFF | **1.57:1** | FAIL |
| Dark | #212F4B | #101826 | **1.33:1** | FAIL |

Both values are essentially invisible against their backgrounds, making the breadcrumb navigation harder to follow.

**Usage (line 1814):**
```css
border-bottom: 1px solid var(--nav-breadcrumb-separator-color);
```

## Recommended Fix
Update the breadcrumb separator colors to meet WCAG AA standards (minimum 3:1 contrast ratio):

**For light mode (line 74):**
```css
--nav-breadcrumb-separator-color: #9CAFD4;  /* Contrast ratio: ~2.5:1, still low */
```

Better option:
```css
--nav-breadcrumb-separator-color: #728DC1;  /* Contrast ratio: ~3.5:1, passes */
```

**For dark mode (line 271):**
```css
--nav-breadcrumb-separator-color: #4665A2;  /* Contrast ratio: ~3.2:1, passes */
```

Alternatively, use a more visible color that matches the design system:
```css
--nav-breadcrumb-separator-color: #5373B4;  /* Contrast ratio: ~4.0:1, passes */
```

The fix involves updating 2 lines in the CSS file and ensures navigation breadcrumbs are clearly visible to all users.

## References
- [WCAG 2.1 - Contrast (Minimum) - UI Components](https://www.w3.org/WAI/WCAG21/Understanding/contrast-minimum.html)
- [WebAIM Contrast Checker](https://webaim.org/resources/contrastchecker/)
- [MDN - CSS Custom Properties](https://developer.mozilla.org/en-US/docs/Web/CSS/--*)

</content>