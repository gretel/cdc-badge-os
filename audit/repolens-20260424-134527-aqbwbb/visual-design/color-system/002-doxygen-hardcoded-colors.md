---
title: "[MEDIUM] Hardcoded color literals in doxygen CSS instead of CSS variables"
severity: MEDIUM
domain: visual-design
lens: color-system
labels:
  - "audit:visual-design/color-system"
---

## Summary
The doxygen documentation CSS (`doxygen_output/html/doxygen.css`) uses hardcoded color literals like `white` and `black` in some places instead of referencing the defined CSS variables, making theme consistency harder to maintain.

**File:** `doxygen_output/html/doxygen.css` (lines 5, 6, 105, 202)

## Impact
- **Theme consistency:** Hardcoded values bypass the centralized color palette, creating potential for visual inconsistency
- **Maintenance:** When updating the color scheme, hardcoded values may be missed
- **Dark mode reliability:** Manual literals may not adapt properly to different themes

## Evidence
CSS variables are defined but not consistently used:

**Lines 5-6 (Light mode):**
```css
--page-background-color: white;
--page-foreground-color: black;
```

**Line 105 (Light mode search):**
```css
--search-background-color: white;
--search-active-color: black;
```

**Lines 202-203 (Dark mode):**
```css
--page-background-color: black;
--page-foreground-color: #C9D1D9;
```

**Line 302 (Dark mode search):**
```css
--search-background-color: black;
```

The issue is that `white` and `black` are used as literal values instead of being defined as variables themselves. While this works for basic themes, it makes the system less flexible for custom themes or when extending the color system.

Additionally, line 26 uses:
```css
--glow-color: cyan;
```

This is a named color rather than a hex/RGB value, which may render inconsistently across different browsers and themes.

## Recommended Fix
1. Define base neutral colors as variables:
```css
:root {
  --color-white: #FFFFFF;
  --color-black: #000000;
  --color-cyan: #00FFFF;
}
```

2. Update the color definitions to use these variables:
```css
--page-background-color: var(--color-white);
--page-foreground-color: var(--color-black);
--search-background-color: var(--color-white);
--search-active-color: var(--color-black);
--glow-color: var(--color-cyan);
```

This creates a more complete and consistent color system that can be easily extended or modified.

## References
- [CSS Custom Properties for Color Systems](https://css-tricks.com/using-css-custom-properties-color-systems/)
- [W3C CSS Color Module Level 4](https://www.w3.org/TR/css-color-4/)
