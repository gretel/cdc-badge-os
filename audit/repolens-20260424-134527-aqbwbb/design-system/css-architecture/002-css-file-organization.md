---
title: "[LOW] No CSS file organization - all styles embedded in single HTML file"
severity: LOW
domain: design-system
lens: css-architecture
labels:
  - "file-organization"
  - "maintainability"
---

## Summary
The web-flasher component has all CSS embedded in a `<style>` block within `web-flasher/index.html` (lines 11-216, ~205 lines of CSS). There is no separation between:
- Base/reset styles
- Component styles
- Utility/helper styles
- Media queries

## Impact
- **Scalability**: As the web-flasher grows, the single file will become harder to manage
- **Reusability**: Styles cannot be easily shared with other pages if the project expands
- **Team collaboration**: Multiple developers working on styles will have more merge conflicts
- **Caching**: Browser cannot cache a separate CSS file

## Evidence
Current structure in `web-flasher/index.html`:
```html
<head>
  ...
  <style>
    :root {
      --bg: #0d1117;
      --surface: #161b22;
      ...
    }

    * { margin: 0; padding: 0; box-sizing: border-box; }

    body { ... }
    .container { ... }
    header { ... }
    ...
    @media (max-width: 480px) { ... }
  </style>
</head>
```

All 205 lines of CSS are in one block with no logical file separation.

## Recommended Fix
Since this is a single-page application, the overhead of multiple files may not be justified. However, for better organization:

1. **Option A (Simple)**: Keep single file but add clear section comments:
   ```css
   /* === CSS VARIABLES === */
   :root { ... }

   /* === RESET === */
   * { ... }

   /* === LAYOUT === */
   .container { ... }

   /* === COMPONENTS === */
   .card { ... }
   .steps { ... }

   /* === MEDIA QUERIES === */
   @media (max-width: 480px) { ... }
   ```

2. **Option B (Best for growth)**: Extract to `web-flasher/styles.css`:
   ```html
   <head>
     <link rel="stylesheet" href="styles.css">
   </head>
   ```

Given the page size (~300 lines total), Option A with comments is likely sufficient.

## References
- [Google CSS Style Guide - File organization](https://google.github.io/styleguide/cssguide.html#File-organization)
- [MDN: CSS best practices](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_laying_out/CSS_styling_basics)
