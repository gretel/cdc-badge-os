---
title: "[MEDIUM] Web flasher lacks system theme detection and `color-scheme` support"
severity: MEDIUM
domain: adaptive-ux/theme-adaptation
lens: theme-adaptation
labels:
  - dark-mode
  - system-preference
---

## Summary
The web flasher (`web-flasher/index.html`) has a hardcoded dark theme but does not:
1. Detect system color scheme preference via `prefers-color-scheme` media query or `matchMedia`
2. Set the `color-scheme` CSS property on `:root` or `html` for browser-native controls
3. Allow users to toggle between light/dark/system themes

The CSS defines only dark-mode colors (lines 12-22):
```css
:root {
  --bg: #0d1117;
  --surface: #161b22;
  --border: #30363d;
  --text: #e6edf3;
  ...
}
```

## Impact
- Users with light-mode OS settings see a dark interface by default (potential contrast issues)
- Browser-native UI controls (scrollbars, form inputs, date pickers, dialogs) remain in light mode despite the dark theme
- No way for users to override or match their system preference
- Third-party Web Serial API dialogs may have inconsistent theming

## Evidence
**File: `web-flasher/index.html`**
- Lines 12-22: Only dark-mode CSS custom properties defined
- Lines 11-216: `<style>` block with no `@media (prefers-color-scheme: dark)` or `@media (prefers-color-scheme: light)` queries
- No `color-scheme` property anywhere in the CSS
- No JavaScript `matchMedia` listener for system preference detection

## Recommended Fix
1. **Add `color-scheme` support:**
```css
:root {
  color-scheme: light dark;
  /* existing dark mode variables */
  --bg: #0d1117;
  --surface: #161b22;
  ...
}

:root[data-theme="light"] {
  --bg: #ffffff;
  --surface: #f6f8fa;
  --border: #d0d7de;
  --text: #24292f;
  ...
}

@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
    --bg: #0d1117;
    --surface: #161b22;
    ...
  }
}
```

2. **Add system preference detection in JavaScript:**
```javascript
function getInitialTheme() {
  const stored = localStorage.getItem('theme');
  if (stored) return stored;
  return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
}

document.documentElement.setAttribute('data-theme', getInitialTheme());
```

3. **Add theme toggle UI** for user override capability.

## References
- [MDN: color-scheme](https://developer.mozilla.org/en-US/docs/Web/CSS/color-scheme)
- [MDN: prefers-color-scheme](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/prefers-color-scheme)
- [Inclusive Components: Colour Themes](https://inclusive-components.design/colour-themes/)
