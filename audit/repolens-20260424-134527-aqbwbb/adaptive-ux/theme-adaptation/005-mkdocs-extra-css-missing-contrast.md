---
title: "[LOW] MKDocs extra.css has incomplete dark mode link color definition"
severity: LOW
domain: adaptive-ux/theme-adaptation
lens: theme-adaptation
labels:
  - dark-mode
  - documentation
---

## Summary
The MKDocs extra stylesheet (`third_party/libtropic/docs/stylesheets/extra.css`) defines a dark mode link color but only for `[data-md-color-scheme="slate"]` (line 8-10). The variable `--md-typeset-a-color` is set but this may not cover all link states (hover, active, focus) and there's no corresponding light-mode override for completeness.

## Evidence
**File: `third_party/libtropic/docs/stylesheets/extra.css`**

```css
/* Dark mode link color */
[data-md-color-scheme="slate"] {
  --md-typeset-a-color: #6584ff; /* Replace with your preferred dark mode color */
}
```

Issues:
1. Comment says "Replace with your preferred dark mode color" - placeholder text left in production
2. Only defines dark mode; light mode uses default which may not match design
3. No hover/active/focus state handling for links
4. No `prefers-color-scheme` media query for automatic system detection

## Impact
- Inconsistent link colors if the default light mode color doesn't match the design
- Users switching between light/dark may see abrupt color changes
- The comment suggests this was a TODO that was never completed

## Recommended Fix
Complete the theme color definitions:

```css
:root {
  /* Light mode link color (if different from default) */
  --md-typeset-a-color: #0032FF;
}

[data-md-color-scheme="slate"] {
  --md-typeset-a-color: #6584ff;
}

/* Ensure hover states are visible in both themes */
[data-md-color-scheme="slate"] a:hover {
  --md-typeset-a-color: #8ab4f8;
}

/* Optional: Auto-detect system preference */
@media (prefers-color-scheme: dark) {
  :root:not([data-md-color-scheme="slate"]) {
    --md-typeset-a-color: #6584ff;
  }
}
```

## References
- [Material for MkDocs: Theming](https://squidfunk.github.io/mkdocs-material/setup/changing-the-colors/)
