---
title: "[LOW] Doxygen documentation missing link URL expansion for print"
severity: LOW
domain: adaptive-ux
lens: print-stylesheet
labels:
  - audit:adaptive-ux/print-stylesheet
---

## Summary
The Doxygen-generated documentation (`doxygen_output/html/`) has basic `@media print` rules but lacks link URL expansion. When users print documentation pages, hyperlinks do not show their destination URLs, making printed references less useful.

**File:** `doxygen_output/html/doxygen.css`
**Print rules location:** Lines 1-17 of `@media print` block

## Impact
- Printed documentation links become plain text without destinations
- Users cannot follow references from printed pages
- Code examples with cross-references lose context

## Evidence
The existing `@media print` block in `doxygen.css`:
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
	#doc-content
	{
	margin-left:0 !important;
	height:auto !important;
	width:auto !important;
	overflow:inherit;
	display:inline;
	}
}
```

Missing: No `a[href]:after` rule to append URLs.

## Recommended Fix
Add to the `@media print` block in `doxygen.css`:

```css
/* Link URL expansion for print */
a[href]:after {
	content: " (" attr(href) ")";
	font-size: 80%;
}

/* Exclude internal anchors and JavaScript links */
a[href^="#"]:after,
a[href^="javascript:"]:after {
	content: "";
}
```

## References
- [W3C CSS Content Module](https://www.w3.org/TR/css-content-3/)
- Best practice for printable documentation
