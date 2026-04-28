---
title: "[MEDIUM] Web flasher missing `color-scheme` CSS property for native browser UI"
severity: MEDIUM
domain: adaptive-ux/theme-adaptation
lens: theme-adaptation
labels:
  - dark-mode
  - system-preference
  - color-scheme
---

## Summary
The web flasher (`web-flasher/index.html`) defines a dark theme via CSS custom properties but does not set the `color-scheme` CSS property on the root element. This means browser-native UI controls (scrollbars, form inputs, select dropdowns, date pickers, dialogs) will render in light mode despite the page's dark theme.

## Impact
- Scrollbars appear in light mode (white/gray) on a dark background, creating visual inconsistency
- Web Serial API permission dialogs and other browser-native controls may use light colors
- Users with light-mode OS settings may experience contrast issues with native controls
- The `color-scheme` property also helps the browser optimize rendering for the correct theme

## Evidence
**File: `web-flasher/index.html`**

Lines 12-22 define the CSS custom properties but no `color-scheme`:
```css
:root {
  --bg: #0d1117;
  --surface: #161b22;
  --border: #30363d;
  --text: #e6edf3;
  --text-muted: #8b949e;
  --accent: #58a6ff;
  --accent-hover: #79c0ff;
  --warning: #d29922;
  --step-bg: #1c2128;
}
```

No `color-scheme: dark;` or `color-scheme: light dark;` is defined anywhere in the `<style>` block.

## Recommended Fix
Add the `color-scheme` property to the `:root` selector:

```css
:root {
  color-scheme: dark;
  --bg: #0d1117;
  --surface: #161b22;
  /* ... rest of variables */
}
```

If you plan to add light mode support in the future, use:
```css
:root {
  color-scheme: light dark;
  --bg: #0d1117;
  /* ... rest of variables */
}

:root[data-theme="light"] {
  color-scheme: light;
  --bg: #ffffff;
  /* ... light mode variables */
}
```

This single line tells the browser to render native UI elements (scrollbars, form controls, etc.) with dark-mode colors.

## References
- [MDN: color-scheme](https://developer.mozilla.org/en-US/docs/Web/CSS/color-scheme)
- [CSS-Tricks: A Complete Guide to color-scheme](https://css-tricks.com/a-complete-guide-to-color-scheme/)
- [Web.dev: Optimize text visibility](https://web.dev/optimize-text-visibility/#avoid-flash-of-native-styling)

</content>