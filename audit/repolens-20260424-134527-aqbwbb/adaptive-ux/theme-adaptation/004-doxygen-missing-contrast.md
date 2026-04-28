---
title: "[LOW] Doxygen documentation lacks `prefers-contrast` support"
severity: LOW
domain: adaptive-ux/theme-adaptation
lens: theme-adaptation
labels:
  - accessibility
  - contrast
---

## Summary
The doxygen documentation CSS files (`doxygen_output/html/doxygen.css` and `third_party/libtropic/docs/doxygen/html/doxygen-awesome.css`) have dark mode support via `prefers-color-scheme` but do not include any `prefers-contrast` media query handling for users who need high contrast mode.

## Impact
Users with visual impairments who enable "High Contrast" mode in their OS may experience:
- Insufficient color separation for readability
- Focus rings or borders that are too subtle
- Text that blends too much with background

While this is a documentation site (not the main application), improving contrast handling would improve accessibility for documentation readers.

## Evidence
**Files checked:**
- `doxygen_output/html/doxygen.css` - Has `@media (prefers-color-scheme: dark)` (line 197) but no `@media (prefers-contrast: more)`
- `third_party/libtropic/docs/doxygen/html/doxygen-awesome.css` - Same pattern

No `prefers-contrast: more` or `prefers-contrast: less` queries exist in either file.

## Recommended Fix
Add high-contrast overrides to the existing dark mode media queries:

```css
@media (prefers-color-scheme: dark) {
  html:not(.light-mode) {
    color-scheme: dark;
    /* existing dark mode variables */
  }
}

@media (prefers-contrast: more) {
  /* Increase contrast for high-contrast mode */
  :root {
    --page-foreground-color: #ffffff;
    --separator-color: #ffffff;
    --link-color: #8ab4f8;
  }

  a {
    text-decoration: underline;
    font-weight: 600;
  }

  *:focus {
    outline: 3px solid #fff;
    outline-offset: 2px;
  }

  table, .memitem {
    border-width: 2px;
  }
}
```

## References
- [MDN: prefers-contrast](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/prefers-contrast)
- [W3C: Media Feature: prefers-contrast](https://www.w3.org/TR/mediaqueries-5/#prefers-contrast)
