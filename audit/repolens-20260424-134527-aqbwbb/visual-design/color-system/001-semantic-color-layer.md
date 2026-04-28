---
title: "[MEDIUM] Missing semantic color layer in web-flasher"
severity: MEDIUM
domain: visual-design
lens: color-system
labels:
  - "audit:visual-design/color-system"
---

## Summary
The web-flasher component (`web-flasher/index.html`) defines colors using hue-based naming (`--bg`, `--accent`, `--text`) rather than semantic purpose-based naming (`--color-surface`, `--color-primary`, `--color-text`). This makes it harder to maintain theme coherence and apply consistent design tokens across the application.

**File:** `web-flasher/index.html` (lines 12-22)

## Impact
- **Maintainability:** When colors need to be adjusted for branding or accessibility, developers must understand the visual role of each color rather than its semantic purpose
- **Theme consistency:** Hard-to-rename variables increase the risk of inconsistent color usage across different components
- **Accessibility:** Semantic naming makes it easier to audit contrast ratios and ensure WCAG compliance

## Evidence
Current color definitions in `web-flasher/index.html`:
```css
:root {
  --bg: #0d1117;           /* Should be --color-background */
  --surface: #161b22;      /* Good - this is semantic */
  --border: #30363d;       /* Should be --color-border */
  --text: #e6edf3;         /* Should be --color-text */
  --text-muted: #8b949e;   /* Good - this is semantic */
  --accent: #58a6ff;       /* Should be --color-primary */
  --accent-hover: #79c0ff; /* Should be --color-primary-hover */
  --warning: #d29922;      /* Good - this is semantic */
  --step-bg: #1c2128;      /* Should be --color-step-background */
}
```

Inconsistent naming pattern:
- `--bg` (visual) vs `--surface` (semantic)
- `--accent` (visual) vs `--warning` (semantic)
- `--text` (semantic) mixed with `--bg` (visual)

## Recommended Fix
Rename color variables to use a consistent semantic naming convention:

```css
:root {
  --color-background: #0d1117;
  --color-surface: #161b22;
  --color-border: #30363d;
  --color-text: #e6edf3;
  --color-text-muted: #8b949e;
  --color-primary: #58a6ff;
  --color-primary-hover: #79c0ff;
  --color-warning: #d29922;
  --color-step-background: #1c2128;
}
```

Then update all references throughout the file (approximately 20-25 replacements).

## References
- [Design Tokens - W3C Community Group](https://w3c.github.io/design-tokens/)
- [Semantic Color Systems in CSS](https://www.smashingmagazine.com/2021/07/semantic-color-systems-css/)
- [Material Design Color System](https://m3.material.io/styles/color/the-color-system/color-tokens)
