---
title: "[LOW] Doxygen links missing :focus states for keyboard navigation"
severity: LOW
domain: frontend
lens: responsive-design
labels:
  - responsive-design
  - accessibility
  - keyboard-navigation
---

## Summary
The doxygen documentation CSS (`doxygen_output/html/doxygen.css`) defines link styles with `:hover` and `:visited` states but lacks `:focus` states for keyboard navigation. Only the main menu has a focus state.

**Location**: `doxygen_output/html/doxygen.css:607-611`

```css
a {
	color: var(--page-link-color);
	font-weight: normal;
	text-decoration: none;
}
```

Links have no underline by default and only show hover effects in some cases. Keyboard users get no visual indication of the focused link.

## Impact
**Minor accessibility impact**:
- Keyboard users navigating with Tab key don't get clear focus indication
- Screen reader users may not know which link is currently focused
- Doesn't meet WCAG 2.1 Level AA requirements for visible focus (2.4.7 Focus Visible)
- Inconsistent experience between mouse and keyboard users

## Evidence
**Current link states** (lines 607-620):
```css
a {
	color: var(--page-link-color);
	font-weight: normal;
	text-decoration: none;
}

.contents a:visited {
	color: var(--page-visited-link-color);
}

span.label a:hover {
	text-decoration: none;
	background:   linear-gradient(to bottom, transparent 0,transparent calc(100% - 1px), currentColor 100%);
}
```

**Only one focus state exists** (line 553):
```css
#main-menu a:focus {
	outline: auto;
	z-index: 10;
	position: relative;
}
```

All other links lack `:focus` or `:focus-visible` states.

## Recommended Fix
Add focus states for all links:

```css
a {
	color: var(--page-link-color);
	font-weight: normal;
	text-decoration: none;
}

a:hover,
a:focus,
a:focus-visible {
	text-decoration: underline;
}

.contents a:visited {
	color: var(--page-visited-link-color);
}

.contents a:visited:hover,
.contents a:visited:focus {
	text-decoration: underline;
}

/* Optional: Add outline for better visibility */
a:focus {
	outline: 2px solid var(--page-link-color);
	outline-offset: 2px;
}

/* More specific for different link types */
.memItemLeft a:focus,
.memItemRight a:focus,
.mdescLeft a:focus,
.mdescRight a:focus {
	outline: 2px solid var(--page-link-color);
	outline-offset: -2px;
}
```

## References
- [WCAG 2.1 - Focus Visible (2.4.7)](https://www.w3.org/WAI/WCAG21/Understanding/focus-visible.html)
- [MDN - :focus-visible pseudo-class](https://developer.mozilla.org/en-US/docs/Web/CSS/:focus-visible)
- [Web.dev - Focus visible](https://web.dev/focus-visible/)
- [CSS Tricks - Complete Guide to Link Pseudo-classes](https://css-tricks.com/complete-guide-link-pseudo-classes/)
