---
title: "[MEDIUM] Web flasher has potential flash of wrong theme (FOWT) on load"
severity: MEDIUM
domain: adaptive-ux/theme-adaptation
lens: theme-adaptation
labels:
  - dark-mode
  - theme-flash
---

## Summary
The web flasher (`web-flasher/index.html`) applies theme colors only via CSS in the `<style>` block, but there is no mechanism to prevent a flash of the default (light) theme before the dark theme is applied. The theme is defined in CSS but there is:
1. No inline script in `<head>` to set theme class/attribute before first paint
2. No `color-scheme` property to tell the browser what theme to use natively
3. No theme persistence (localStorage) to remember user preference across sessions

## Impact
- On slow connections or fast renders, users may see a brief flash of white/light background before dark theme applies
- Theme preference is lost on page refresh, requiring users to manually re-apply if they want light mode
- Inconsistent experience across page navigations or revisits

## Evidence
**File: `web-flasher/index.html`**
- Lines 11-216: All CSS is in a `<style>` block, but no inline blocking script
- Lines 284-301: JavaScript only fetches version info, no theme initialization
- No `localStorage` read/write for theme preference
- No `color-scheme` property in CSS

The page relies purely on CSS variable defaults which are dark, but without `color-scheme: dark` the browser may render form controls and scrollbars in light mode initially.

## Recommended Fix
Add an inline blocking script in `<head>` before the CSS:

```html
<head>
  ...
  <script>
    (function() {
      const stored = localStorage.getItem('theme');
      const systemDark = window.matchMedia('(prefers-color-scheme: dark)').matches;
      const theme = stored || (systemDark ? 'dark' : 'light');
      document.documentElement.classList.add(theme);
      document.documentElement.style.colorScheme = theme;
    })();
  </script>
  <style>
    :root {
      color-scheme: light dark;
      ...
    }
  </style>
</head>
```

This ensures:
- Theme is applied before first paint (no FOWT)
- System preference is respected on first visit
- Stored preference is restored on revisits

## References
- [Web.dev: Optimize text visibility](https://web.dev/optimize-text-visibility/#avoid-flash-of-native-styling)
- [CSS-Tricks: Preventing Flash of Wrong Theme](https://css-tricks.com/flash-of-wrong-theme-improved/)
