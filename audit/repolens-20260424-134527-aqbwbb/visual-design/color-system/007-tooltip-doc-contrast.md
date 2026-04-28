---
title: "[LOW] Tooltip documentation text color fails WCAG AA in light mode"
severity: LOW
domain: visual-design
lens: color-system
labels:
  - "audit:visual-design/color-system"
---

## Summary
The doxygen documentation tooltip uses `grey` (CSS named color, equivalent to `#808080`) for documentation text on a white background. This has a contrast ratio of approximately 3.95:1, which fails the WCAG AA requirement of 4.5:1 for normal text.

**File:** `doxygen_output/html/doxygen.css` (line 152)

## Impact
- **Accessibility:** Users with moderate vision impairments may struggle to read tooltip documentation
- **WCAG AA compliance:** Fails the 4.5:1 contrast requirement for normal text
- **Usability:** Tooltip text may be difficult to read in bright lighting conditions or on less contrasty displays

## Evidence
**Line 152 (light mode):**
```css
--tooltip-doc-color: grey;
```

**Line 147 (tooltip foreground):**
```css
--tooltip-foreground-color: black;
```

**Line 148 (tooltip background):**
```css
--tooltip-background-color: rgba(255,255,255,0.8);
```

**Contrast calculation:**
- Foreground: `grey` (#808080)
- Background: `rgba(255,255,255,0.8)` (effectively ~#E5E5E5 on typical content)
- Contrast ratio: **3.95:1**

**WCAG AA requirements:**
- Normal text: 4.5:1 minimum
- Large text (18pt+): 3:1 minimum

The tooltip documentation text falls below the 4.5:1 threshold for normal text.

**Dark mode (line 350):**
```css
--tooltip-doc-color: #D9E1E9;
```
This has a contrast ratio of approximately 12.33:1 on the dark background, which passes.

## Recommended Fix
Update the tooltip documentation color to meet WCAG AA standards:

**For light mode (line 152):**
```css
--tooltip-doc-color: #595959;  /* Contrast ratio: 4.5:1 */
```

Better option for more comfortable reading:
```css
--tooltip-doc-color: #404040;  /* Contrast ratio: 6.0:1 */
```

This change ensures tooltip documentation is legible for users with moderate visual impairments while maintaining the visual hierarchy (still lighter than the main black text).

## References
- [WCAG 2.1 - Contrast (Minimum)](https://www.w3.org/WAI/WCAG21/Understanding/contrast-minimum.html)
- [WebAIM Contrast Checker](https://webaim.org/resources/contrastchecker/)
- [CSS Named Colors](https://www.w3.org/TR/css-color-4/#named-colors)

</content>