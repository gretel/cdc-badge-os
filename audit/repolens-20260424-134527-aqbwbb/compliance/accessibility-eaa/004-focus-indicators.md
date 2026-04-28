---
title: "[MEDIUM] Focus indicator styles may lack sufficient contrast"
severity: MEDIUM
domain: web-flasher
lens: accessibility-eaa
labels:
  - "wcag-2.1"
  - "eaa"
  - "bfsg"
  - "color-contrast"
  - "keyboard-navigation"
---

## Summary
The web flasher CSS at `web-flasher/index.html` (lines 165-176) defines custom button styles but does not explicitly define focus indicators. The browser default focus outline may have insufficient contrast against the dark theme background.

## Impact
Users navigating via keyboard need visible focus indicators to know which element is currently selected. The current button styles use:
- Background: `var(--accent)` (#58a6ff)
- Text: `var(--bg)` (#0d1117)

When focused, the default browser outline may not meet the 3:1 contrast ratio required for focus indicators (WCAG 2.1 SC 2.4.7).

## Evidence
File: `web-flasher/index.html` (lines 165-176):
```css
esp-web-install-button button {
  background: var(--accent);
  color: var(--bg);
  border: none;
  border-radius: 0.5rem;
  padding: 0.85rem 2rem;
  font-size: 1rem;
  font-weight: 600;
  cursor: pointer;
  transition: background 0.15s;
}

esp-web-install-button button:hover {
  background: var(--accent-hover);
}
```

No `:focus` or `:focus-visible` styles are defined.

## Recommended Fix
Add explicit focus styles with sufficient contrast:

```css
esp-web-install-button button:focus-visible {
  outline: 2px solid #fff;
  outline-offset: 2px;
  box-shadow: 0 0 0 4px rgba(88, 166, 255, 0.5);
}
```

Also add focus styles for other interactive elements:
```css
a:focus-visible,
.button:focus-visible {
  outline: 2px solid var(--accent);
  outline-offset: 2px;
}
```

## References
- [WCAG 2.1 SC 2.4.7 Focus Visible](https://www.w3.org/WAI/WCAG21/Understanding/focus-visible.html)
- [WCAG 2.1 SC 1.4.11 Contrast (Non-text)](https://www.w3.org/WAI/WCAG21/Understanding/contrast-minimum.html)
