---
title: "[LOW] Doxygen member labels (mlabel) undersized for touch"
severity: LOW
domain: documentation
lens: touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The member labels in Doxygen documentation (`doxygen_output/html/doxygen.css`) have minimal padding (2px 3px) and small font (7pt), resulting in touch targets that may be undersized for comfortable tapping on mobile devices.

**Location:** `doxygen_output/html/doxygen.css:1422-1436`

## Impact
- **Mobile Usability:** Labels with 2px 3px padding and 7pt font (≈10px) create small touch targets of approximately 16x14px
- **Accessibility:** May not meet WCAG 2.5.8 Target Size (Minimum) guidelines for touch interfaces
- **User Experience:** Small labels in member documentation may be difficult to tap for filtering or navigation

## Evidence
The member label styles in `doxygen.css` (lines 1422-1436):

```css
span.mlabel {
	background-color: var(--label-background-color);
	border-top:1px solid var(--label-left-top-border-color);
	border-left:1px solid var(--label-left-top-border-color);
	border-right:1px solid var(--label-right-bottom-border-color);
	border-bottom:1px solid var(--label-right-bottom-border-color);
	text-shadow: none;
	color: var(--label-foreground-color);
	margin-right: 4px;
	padding: 2px 3px;
	border-radius: 3px;
	font-size: 7pt;
	white-space: nowrap;
	vertical-align: middle;
}
```

The `.mlabel` uses 7pt font (≈10px) with only 2px top/bottom padding and 3px left/right padding. Total touch dimensions are approximately 14px height × 16px width, well below the 44px recommendation.

## Recommended Fix
Add explicit minimum dimensions and increased padding to ensure adequate touch targets:

```css
span.mlabel {
	background-color: var(--label-background-color);
	border-top:1px solid var(--label-left-top-border-color);
	border-left:1px solid var(--label-left-top-border-color);
	border-right:1px solid var(--label-right-bottom-border-color);
	border-bottom:1px solid var(--label-right-bottom-border-color);
	text-shadow: none;
	color: var(--label-foreground-color);
	margin-right: 4px;
	padding: 6px 8px; /* Increased from 2px 3px */
	border-radius: 3px;
	font-size: 7pt;
	white-space: nowrap;
	vertical-align: middle;
	min-height: 24px; /* Minimum practical touch height */
	display: inline-flex;
	align-items: center;
}

/* Optionally increase for coarse pointers */
@media (any-pointer: coarse) {
    span.mlabel {
        padding: 10px 12px;
        min-height: 36px;
    }
}
```

## References
- [WCAG 2.5.8 Target Size (Minimum)](https://www.w3.org/WAI/WCAG21/Understanding/target-size.html)
- [Material Design Touch Targets](https://m3.material.io/foundations/interactions/touch-interaction/touch-targets)
- [MDN: CSS Media Queries - Pointer](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/any-pointer)
