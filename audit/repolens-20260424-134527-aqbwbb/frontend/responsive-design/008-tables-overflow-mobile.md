---
title: "[MEDIUM] Doxygen tables may overflow on mobile screens"
severity: MEDIUM
domain: frontend
lens: responsive-design
labels:
  - responsive-design
  - layout
  - css
---

## Summary
The doxygen documentation tables (`doxygen_output/html/doxygen.css`) lack proper overflow handling for mobile screens. Tables with wide content will cause horizontal scrolling on narrow viewports.

**Location**: `doxygen_output/html/doxygen.css:1692-1709`

```css
table.doxtable {
	border-collapse:collapse;
	margin-top: 4px;
	margin-bottom: 4px;
}

table.doxtable td, table.doxtable th {
	border: 1px solid var(--table-cell-border-color);
	padding: 3px 7px 2px;
}

table.doxtable th {
	background-color: var(--table-header-background-color);
	color: var(--table-header-foreground-color);
	font-size: 110%;
	padding-bottom: 4px;
	padding-top: 5px;
}
```

No `max-width: 100%`, no `overflow-x: auto` wrapper, no `word-wrap` for long content.

## Impact
**Moderate usability impact** on mobile devices:
- Wide tables extend beyond viewport on small screens
- Horizontal scrollbar appears on body level (not table level)
- Difficult to read tables on phones (< 480px width)
- Tables with long code snippets or URLs cause overflow
- No scroll hint for users that table is scrollable

## Evidence
**Current CSS** (lines 1692-1709):
- No `max-width: 100%` on table
- No overflow handling
- No word-wrapping for long content

**Comparison**: The doxygen-awesome.css properly handles tables:
```css
/* doxygen-awesome.css:1644-1648 */
.contents table:not(.memberdecls):not(.mlabels):not(.fieldtable):not(.memname),
.contents table:not(.memberdecls):not(.mlabels):not(.fieldtable):not(.memname) tbody {
    display: inline-block;
    max-width: 100%;
}

/* doxygen-awesome.css:2332-2336 */
.contents table:not(.memberdecls):not(.mlabels):not(.fieldtable):not(.memname) tbody {
    overflow-x: auto;
    overflow-x: overlay;
}
```

## Recommended Fix
Add responsive table styling with overflow handling:

```css
/* Make tables scrollable on mobile */
table.doxtable {
	border-collapse:collapse;
	margin-top: 4px;
	margin-bottom: 4px;
	max-width: 100%;
	overflow-x: auto;
	display: block;
}

/* Alternative: Wrap tables in scrollable container */
div.contents table.doxtable {
	max-width: 100%;
}

/* Allow word wrapping for long content */
table.doxtable td, table.doxtable th {
	border: 1px solid var(--table-cell-border-color);
	padding: 3px 7px 2px;
	word-wrap: break-word;
	overflow-wrap: break-word;
}

/* Mobile-specific adjustments */
@media screen and (max-width: 767px) {
	table.doxtable {
		font-size: 0.9rem;
	}
	
	table.doxtable td, table.doxtable th {
		padding: 2px 4px;
	}
}
```

Or use a wrapper approach:
```css
/* Add scrollable wrapper for wide tables */
table.doxtable {
	display: inline-table;
	max-width: 100%;
}

.contents > table.doxtable {
	margin-left: 0;
	margin-right: 0;
}
```

## References
- [MDN - CSS Overflow](https://developer.mozilla.org/en-US/docs/Web/CSS/overflow)
- [CSS Tricks - Responsive Tables](https://css-tricks.com/responsive-tables/)
- [Web Accessibility - Reflow (WCAG 2.4)](https://www.w3.org/WAI/WCAG21/Understanding/reflow.html)
- [Google - Tables on Mobile](https://developers.google.com/web/fundamentals/design-and-ux/responsive/patterns#tables)
