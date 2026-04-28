---
title: "[MEDIUM] No centralized box-shadow/elevation scale - inconsistent shadow definitions"
severity: MEDIUM
domain: visual-design
lens: visual-hierarchy
labels:
  - "audit:visual-design/visual-hierarchy"
---

## Summary
Box-shadow values are defined in multiple places without a unified elevation scale. While `doxygen-awesome.css` defines a `--box-shadow` variable, it is used inconsistently and overridden in various places with hardcoded values. Evidence:

**Defined but not unified:**
- `doxygen_output/html/doxygen.css` - Multiple inline box-shadows
- `third_party/libtropic/docs/doxygen/html/doxygen-awesome.css:58` - `--box-shadow: 0 2px 8px 0 rgba(0,0,0,.075)`

**Hardcoded variations:**
- `doxygen_output/html/doxygen.css:2218` - `box-shadow: 2px 2px 2px rgba(0, 0, 0, 0.15)` (breadcrumb)
- `doxygen_output/html/doxygen.css:2237` - `box-shadow: 0 0 15px var(--glow-color)` (glow effect)
- `doxygen_output/html/doxygen.css:1850` - `box-shadow: 0 10px 0 -1px var(--memdef-proto-background-color)`
- `doxygen_output/html/doxygen.css:1100` - `box-shadow: 0 0 0 1px var(--separator-color)` (tables)
- `third_party/libtropic/docs/doxygen/html/doxygen-awesome.css:488` - `box-shadow: var(--box-shadow)` (dropdowns)
- `third_party/libtropic/docs/doxygen/html/doxygen-awesome-sidebar-only.css:87` - `box-shadow: 0 calc(-2 * var(--top-height)) 0 0 var(--separator-color)`

## Impact
1. **Inconsistent elevation**: Cards, modals, dropdowns have different shadow depths for the same conceptual elevation level.
2. **Visual noise**: Competing shadow values create unpredictable depth perception.
3. **Maintenance burden**: Changing the shadow theme requires editing multiple files.
4. **Contradicting shadows**: Lower-layer elements may cast stronger shadows than higher-layer ones (e.g., breadcrumb `2px 2px 2px` vs dropdown `0 2px 8px`).

## Evidence
```css
/* Multiple different shadow definitions for similar purposes */

/* doxygen_output/html/doxygen.css:2218 - breadcrumb */
.navpath li.navelem a {
    box-shadow: 2px 2px 2px rgba(0, 0, 0, 0.15);
}

/* doxygen_output/html/doxygen.css:1100 - tables */
table.fieldtable, table.markdownTable tbody {
    box-shadow: 0 0 0 1px var(--separator-color);
}

/* third_party/libtropic/docs/doxygen/html/doxygen-awesome.css:488 - dropdowns */
.sm-dox ul {
    box-shadow: var(--box-shadow);  /* 0 2px 8px 0 rgba(0,0,0,.075) */
}
```

## Recommended Fix
1. **Define a unified elevation scale** in `doxygen-awesome.css`:
```css
:root {
    --elevation-1: 0 1px 2px rgba(0,0,0,0.05);
    --elevation-2: 0 2px 4px rgba(0,0,0,0.07);
    --elevation-3: 0 4px 8px rgba(0,0,0,0.1);
    --elevation-4: 0 8px 16px rgba(0,0,0,0.15);
    --elevation-5: 0 12px 24px rgba(0,0,0,0.2);
}
```

2. **Replace all hardcoded shadows** with elevation tokens:
```css
.navpath li.navelem a {
    box-shadow: var(--elevation-1);
}

table.fieldtable {
    box-shadow: var(--elevation-2);
}

.sm-dox ul {
    box-shadow: var(--elevation-3);
}
```

3. **Document the elevation scale** with visual examples showing which components use which elevation level.

## References
- [Material Design Elevation](https://m3.material.io/styles/elevation)
- [MDN: box-shadow](https://developer.mozilla.org/en-US/docs/Web/CSS/box-shadow)
