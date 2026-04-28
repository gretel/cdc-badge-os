---
title: "[MEDIUM] Web flasher lacks focus styles for keyboard navigation"
severity: MEDIUM
domain: adaptive-ux/theme-adaptation
lens: theme-adaptation
labels:
  - accessibility
  - focus
  - keyboard
---

## Summary
The web flasher (`web-flasher/index.html`) has no focus styles defined for interactive elements. The main flash button (`esp-web-install-button button`) and all links lack visible focus indicators when navigated via keyboard.

## Impact
- Keyboard-only users cannot see which element is currently focused
- Users navigating with Tab/Shift+Tab will lose their place in the interface
- WCAG 2.1 Level AA compliance gap (Criterion 2.4.7: Focus Visible)
- Poor accessibility for motor-impaired users who rely on keyboard navigation

## Evidence
**File: `web-flasher/index.html`**

Lines 156-169 define the flash button styles but include no focus state:
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

No `:focus` or `:focus-visible` styles exist anywhere in the file. Links in the footer also lack focus styles (lines 200-205).

## Recommended Fix
Add focus styles for all interactive elements:

```css
/* Focus style for flash button */
esp-web-install-button button:focus-visible {
  outline: 2px solid var(--bg);
  outline-offset: 2px;
}

/* Focus style for links */
footer a:focus-visible {
  outline: 2px solid var(--accent);
  outline-offset: 2px;
}

/* General focus-visible for all focusable elements */
*:focus-visible {
  outline: 2px solid var(--accent);
  outline-offset: 2px;
}

/* Remove default focus ring for cleaner appearance */
*:focus {
  outline: none;
}
```

Using `:focus-visible` instead of `:focus` provides better UX - it shows the outline only for keyboard users, not mouse users who have hover states.

## References
- [WCAG 2.1 Criterion 2.4.7: Focus Visible](https://www.w3.org/TR/WCAG21/#focus-visible)
- [MDN: :focus-visible](https://developer.mozilla.org/en-US/docs/Web/CSS/:focus-visible)
- [Inclusive Components: Keyboard Focus](https://inclusive-components.design/keyboard-focus/)

</content>