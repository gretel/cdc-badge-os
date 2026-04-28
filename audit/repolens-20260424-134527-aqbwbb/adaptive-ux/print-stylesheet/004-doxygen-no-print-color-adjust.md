---
title: "[LOW] Doxygen documentation missing print-color-adjust for diagrams"
severity: LOW
domain: adaptive-ux
lens: print-stylesheet
labels:
  - audit:adaptive-ux/print-stylesheet
---

## Summary
The Doxygen documentation (`doxygen_output/html/`) contains diagrams (class hierarchies, inheritance trees, flowcharts) that rely on background colors and may lose visual distinction when printed. The `@media print` block lacks `print-color-adjust` to preserve these colors.

**File:** `doxygen_output/html/doxygen.css`
**Context:** Diagram images and colored status indicators

## Impact
- Diagram backgrounds may print as white, losing contrast
- Color-coded elements (inheritance arrows, member indicators) may become indistinguishable
- Users printing API documentation lose visual context from diagrams

## Evidence
The existing `@media print` block focuses on layout but not color preservation:
```css
@media print
{
	#top { display: none; }
	#side-nav { display: none; }
	#nav-path { display: none; }
	body { overflow:visible; }
	h1, h2, h3, h4, h5, h6 { page-break-after: avoid; }
	.summary { display: none; }
	.memitem { page-break-inside: avoid; }
	#doc-content { margin-left:0 !important; ... }
}
```

Missing: No `print-color-adjust` or `-webkit-print-color-adjust` declarations.

## Recommended Fix
Add to the `@media print` block in `doxygen.css`:

```css
/* Preserve colors for diagrams and indicators */
.memitem, .memproto, .memdoc, .fragment {
	-webkit-print-color-adjust: exact;
	print-color-adjust: exact;
}
```

Or more broadly:
```css
body {
	-webkit-print-color-adjust: economy;
	print-color-adjust: economy;
}
```

## References
- [MDN: print-color-adjust](https://developer.mozilla.org/en-US/docs/Web/CSS/print-color-adjust)
- [CSS Color Module Level 4](https://drafts.csswg.org/css-color-4/#the-print-color-adjust)
