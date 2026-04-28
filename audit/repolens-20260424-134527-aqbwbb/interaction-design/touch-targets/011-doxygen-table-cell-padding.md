---
title: "[LOW] Doxygen table cells have minimal padding for touch"
severity: LOW
domain: documentation
lens: touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
Table cells in Doxygen documentation (`doxygen_output/html/doxygen.css`) have minimal padding (3px 7px 2px), resulting in touch targets that may be undersized for comfortable tapping on mobile devices when tables contain clickable content.

**Location:** `doxygen_output/html/doxygen.css:1696-1702, 1718-1722`

## Impact
- **Mobile Usability:** Table cells with 3px top, 7px left/right, and 2px bottom padding create tight touch areas
- **Accessibility:** May not meet WCAG 2.5.8 Target Size (Minimum) guidelines when cells contain interactive elements
- **User Experience:** Dense tables are harder to navigate on touch devices with limited space

## Evidence
The table cell styles in `doxygen.css` (lines 1696-1702, 1718-1722):

```css
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

And for field tables:

```css
.fieldtable td, .fieldtable th {
  padding: 3px 7px 2px;
}
```

The padding of 3px (top) and 2px (bottom) creates minimal vertical space. For a typical 14px font, the total touch height is approximately 19px, well below the 44px recommendation.

## Recommended Fix
Increase padding for better touch targets, especially on mobile:

```css
table.doxtable td, table.doxtable th {
  border: 1px solid var(--table-cell-border-color);
  padding: 8px 10px 6px; /* Increased from 3px 7px 2px */
}

table.doxtable th {
  background-color: var(--table-header-background-color);
  color: var(--table-header-foreground-color);
  font-size: 110%;
  padding-bottom: 6px;
  padding-top: 8px;
}

.fieldtable td, .fieldtable th {
  padding: 8px 10px 6px; /* Increased from 3px 7px 2px */
}

/* Additional padding for coarse pointers */
@media (any-pointer: coarse) {
  table.doxtable td, table.doxtable th {
    padding: 12px 12px 10px;
  }
  
  table.doxtable th {
    padding-bottom: 10px;
    padding-top: 12px;
  }
  
  .fieldtable td, .fieldtable th {
    padding: 12px 12px 10px;
  }
}
```

## References
- [WCAG 2.5.8 Target Size (Minimum)](https://www.w3.org/WAI/WCAG21/Understanding/target-size.html)
- [Material Design Touch Targets](https://m3.material.io/foundations/interactions/touch-interaction/touch-targets)
- [MDN: CSS Media Queries - Pointer](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/any-pointer)
