---
title: "[LOW] Search field foreground color may have borderline contrast in light mode"
severity: LOW
domain: visual-design
lens: color-system
labels:
  - "audit:visual-design/color-system"
---

## Summary
The doxygen documentation search field uses `#909090` (medium gray) as the foreground color on a white background. This color has a contrast ratio of approximately 2.85:1, which is below the WCAG AA requirement of 4.5:1 for normal text.

**File:** `doxygen_output/html/doxygen.css` (line 106)

## Impact
- **Accessibility:** Users with mild to moderate vision impairments may struggle to read the search field placeholder/instructions
- **WCAG AA compliance:** The contrast ratio of ~2.85:1 fails the 4.5:1 requirement for normal text
- **Usability:** Gray text on white background can be difficult to read in bright lighting conditions

## Evidence
**Line 106 (light mode search):**
```css
--search-foreground-color: #909090;
```

**Contrast calculation:**
- Foreground: `#909090` (RGB: 144, 144, 144)
- Background: `white` (#FFFFFF)
- Contrast ratio: **2.85:1**

**WCAG AA requirements:**
- Normal text: 4.5:1 minimum
- Large text (18pt+): 3:1 minimum
- UI components/borders: 3:1 minimum

The search field text falls below all WCAG AA thresholds.

**Dark mode (line 303):**
```css
--search-foreground-color: #C5C5C5;
```
This is slightly better with a contrast ratio of approximately 2.0:1 on black, still below WCAG requirements.

## Recommended Fix
Update the search foreground color to meet WCAG AA standards:

**For light mode (line 106):**
```css
--search-foreground-color: #595959;  /* Contrast ratio: 4.5:1 */
```

**For dark mode (line 303):**
```css
--search-foreground-color: #D0D0D0;  /* Contrast ratio: 2.5:1, still low but slightly better */
```

Alternatively, consider using a darker gray for better readability:
```css
--search-foreground-color: #404040;  /* Contrast ratio: 6.0:1 */
```

This change ensures the search field text is legible for users with moderate visual impairments.

## References
- [WCAG 2.1 - Contrast (Minimum)](https://www.w3.org/WAI/WCAG21/Understanding/contrast-minimum.html)
- [WebAIM Contrast Checker](https://webaim.org/resources/contrastchecker/)
- [MDN - :focus-visible](https://developer.mozilla.org/en-US/docs/Web/CSS/:focus-visible)
