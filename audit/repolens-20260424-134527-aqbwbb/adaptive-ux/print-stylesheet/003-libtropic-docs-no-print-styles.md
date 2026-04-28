---
title: "[MEDIUM] libtropic documentation missing print-specific styles"
severity: MEDIUM
domain: adaptive-ux
lens: print-stylesheet
labels:
  - audit:adaptive-ux/print-stylesheet
---

## Summary
The libtropic documentation (`third_party/libtropic/docs/`) uses Material for Markdown (based on `extra.css` and `overrides/main.html`) but has no print-specific CSS. The `extra.css` file only defines theme colors and heading weights with no `@media print` rules.

**File:** `third_party/libtropic/docs/stylesheets/extra.css` (lines 1-19)
**Context:** MkDocs Material theme documentation site

## Impact
- Documentation users cannot efficiently print reference material
- Dark mode colors (`--md-primary-fg-color: #0032FF`) may print poorly
- Navigation, sidebar, and TOC will print without being hidden
- Code blocks and API tables may break across pages incorrectly

## Evidence
Current `extra.css` content:
```css
:root  > * {
  --md-primary-fg-color:        #0032FF;
  --md-primary-fg-color--light: #0032FF;
  --md-primary-fg-color--dark:  #0032FF;
}

/* Dark mode link color */
[data-md-color-scheme="slate"] {
  --md-typeset-a-color: #6584ff;
}

.md-typeset h1,
.md-typeset h2,
.md-typeset h3,
.md-typeset h4,
.md-typeset h5,
.md-typeset h6 {
  font-weight: 600;
}
```

No `@media print` block exists. Material for Markdown includes basic print styles by default, but custom overrides may not be print-aware.

## Recommended Fix
Add to `third_party/libtropic/docs/stylesheets/extra.css`:

```css
@media print {
  /* Hide navigation elements */
  .md-nav, .md-header, .md-footer { display: none; }
  
  /* Ensure content fills page */
  .md-main__inner { max-width: none; }
  
  /* Code blocks */
  .md-typeset code { break-inside: avoid; }
  .md-typeset pre { break-inside: avoid; }
  
  /* Tables */
  .md-typeset table { break-inside: avoid; }
  
  /* Headings */
  .md-typeset h1, .md-typeset h2 { break-after: avoid; }
  
  /* Link URLs */
  .md-typeset a[href]:after {
    content: " (" attr(href) ")";
    font-size: 0.8em;
  }
}
```

## References
- [Material for Markdown theming](https://squidfunk.github.io/mkdocs-material/reference/)
- [CSS Paged Media spec](https://www.w3.org/TR/css-page-3/)
