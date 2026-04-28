---
title: "[LOW] Doxygen documentation lacks widows/orphans and @page margin control"
severity: LOW
domain: adaptive-ux
lens: print-stylesheet
labels:
  - audit:adaptive-ux/print-stylesheet
---

## Summary
The Doxygen documentation (`doxygen_output/html/doxygen.css`) has basic `@media print` rules but lacks typographic control for print: no `widows`/`orphans` properties to prevent single-line paragraphs, and no `@page` rule to define consistent margins.

**File:** `doxygen_output/html/doxygen.css`
**Context:** API documentation with long function descriptions and code examples

## Impact
- Single lines of text may appear alone at top/bottom of pages (widows/orphans)
- Browser default margins may be inconsistent across different print setups
- Long function descriptions and parameter lists may have poor page breaks
- Code blocks may split awkwardly across pages

## Evidence
The existing `@media print` block lacks typographic and margin control:
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

Missing:
- No `widows` or `orphans` properties
- No `@page` rule for margins
- No `break-inside: avoid` for code blocks (`.fragment`, `.line`)

## Recommended Fix
Add to the `@media print` block in `doxygen.css`:

```css
/* Prevent widows and orphans */
body {
	widows: 2;
	orphans: 2;
}

/* Define print margins */
@page {
	margin: 2cm;
	margin-top: 2.5cm; /* More space for headers */
	margin-bottom: 2.5cm; /* More space for footers */
}

/* Prevent code block splits */
.fragment, .line, .memProto {
	break-inside: avoid;
}
```

## References
- [CSS Paged Media Module Level 3](https://www.w3.org/TR/css-page-3/)
- [CSS Text Module Level 3 - Widows/Orphans](https://www.w3.org/TR/css-text-3/#widows-orphans)
