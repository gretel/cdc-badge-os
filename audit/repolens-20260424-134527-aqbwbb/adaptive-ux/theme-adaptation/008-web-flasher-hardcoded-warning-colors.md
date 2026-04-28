---
title: "[MEDIUM] Web flasher uses hardcoded warning colors without theme adaptation"
severity: MEDIUM
domain: adaptive-ux/theme-adaptation
lens: theme-adaptation
labels:
  - dark-mode
  - color-adaptation
---

## Summary
The web flasher (`web-flasher/index.html`) uses hardcoded RGBA color values for the warning/notice component that are defined only once without theme adaptation:

- Line 129: `background: rgba(210, 153, 34, 0.1);`
- Line 130: `border: 1px solid rgba(210, 153, 34, 0.3);`

These colors are based on the dark theme's base colors but won't have proper contrast if a light theme is added or if the user's system preference changes.

## Impact
- In light mode, the warning notice with these colors may have insufficient contrast
- The hardcoded colors don't leverage the CSS custom property system (`--warning`, `--text-muted`, etc.)
- Inconsistent theming surface - some elements use variables, others use raw values

## Evidence
**File: `web-flasher/index.html`**

```css
/* Lines 126-135 */
.notice {
  display: flex;
  gap: 0.5rem;
  align-items: flex-start;
  padding: 0.75rem;
  background: rgba(210, 153, 34, 0.1);  /* hardcoded */
  border: 1px solid rgba(210, 153, 34, 0.3);  /* hardcoded */
  border-radius: 0.5rem;
  margin-top: 0.75rem;
  font-size: 0.85rem;
  color: var(--warning);
}
```

The `color: var(--warning)` uses a CSS variable, but the background and border are hardcoded RGBA values that don't adapt.

## Recommended Fix
Replace hardcoded RGBA values with CSS custom properties:

```css
:root {
  /* existing variables */
  --warning: #d29922;
  --warning-bg: rgba(210, 153, 34, 0.1);
  --warning-border: rgba(210, 153, 34, 0.3);
}

:root[data-theme="light"] {
  --warning: #b87310;  /* darker for light background */
  --warning-bg: rgba(184, 115, 16, 0.15);
  --warning-border: rgba(184, 115, 16, 0.4);
}

.notice {
  background: var(--warning-bg);
  border: 1px solid var(--warning-border);
  color: var(--warning);
}
```

Or use the existing `--warning` variable with computed opacity:

```css
.notice {
  background: color-mix(in srgb, var(--warning) 10%, transparent);
  border: 1px solid color-mix(in srgb, var(--warning) 30%, transparent);
  color: var(--warning);
}
```

## References
- [CSS-Tricks: CSS Custom Properties for Dynamic Theming](https://css-tricks.com/a-complete-guide-to-css-custom-properties/)
- [MDN: color-mix()](https://developer.mozilla.org/en-US/docs/Web/CSS/color_value/color-mix)
