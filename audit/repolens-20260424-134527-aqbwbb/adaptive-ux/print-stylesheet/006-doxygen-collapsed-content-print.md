---
title: "[MEDIUM] Doxygen collapsible sections remain collapsed in print"
severity: MEDIUM
domain: adaptive-ux
lens: print-stylesheet
labels:
  - audit:adaptive-ux/print-stylesheet
---

## Summary
The Doxygen documentation (`doxygen_output/html/doxygen.css`) uses JavaScript-powered collapsible sections for inherited members (`.inherit` class with `display: none;`). When printing, these sections remain collapsed, hiding inherited methods and properties from the printed output.

**File:** `doxygen_output/html/doxygen.css`
**Line:** 2246 (`.inherit { display: none; }`)
**Context:** Inherited member documentation sections

## Impact
- Printed documentation is incomplete - inherited members are hidden
- Users printing API reference lose crucial information about inherited methods
- Class hierarchies appear to have fewer methods when printed
- Functionality like "Inherited By" sections collapse content that should be visible in print

## Evidence
The CSS defines collapsible behavior:
```css
.inherit_header {
  font-weight: 400;
  cursor: pointer;
  -webkit-touch-callout: none;
  -webkit-user-select: none;
  /* ... */
}

.inherit {
  display: none;
}
```

The `.inherit` class is applied to inherited member sections that are hidden by default and toggled via JavaScript. In print context, these remain hidden because:
1. The `@media print` block does not override `.inherit { display: none; }`
2. JavaScript interactivity doesn't work in print

## Recommended Fix
Add to the `@media print` block in `doxygen.css`:

```css
/* Expand all collapsed sections for print */
.inherit {
  display: block;
}

/* Make inherit headers static (no cursor pointer in print) */
.inherit_header {
  cursor: default;
  font-weight: bold; /* Emphasize inherited sections */
}

/* Ensure inherited content is visible */
.inherit_header + .inherit {
  display: table-row-group; /* For tabular inherited members */
}
```

## References
- [CSS Paged Media Module](https://www.w3.org/TR/css-page-3/)
- Best practice: All content visible in DOM should print unless explicitly hidden
