---
title: "[LOW] Missing focus-visible states for keyboard navigation"
severity: LOW
domain: frontend
lens: responsive-design
labels:
  - responsive-design
  - accessibility
  - keyboard-navigation
---

## Summary
The web flasher and TinyUSB web interface use `:hover` states but lack `:focus-visible` states for keyboard navigation. This affects users who navigate via keyboard or use assistive technologies.

**Locations**:
- `web-flasher/index.html:208` - Footer link
- `managed_components/espressif__tinyusb/examples/device/webusb_serial/website/style.css:108` - Command history entries

## Impact
**Minor accessibility impact**:
- Keyboard users don't get clear visual feedback for focused elements
- Screen reader users may not know which element is active
- Doesn't meet WCAG 2.1 Level AA requirements for focus visibility
- Different experience for touch vs. keyboard users

## Evidence
**Web Flasher** (`web-flasher/index.html:204-210`):
```css
footer a {
  color: var(--accent);
  text-decoration: none;
}

footer a:hover {
  text-decoration: underline;
}
```

**TinyUSB WebUSB** (`style.css:108-110`):
```css
.command-history-entry:hover {
  background-color: #f0f0f0;
}
```

Neither has `:focus` or `:focus-visible` states.

## Recommended Fix
Add focus-visible states for keyboard navigation:

**Web Flasher**:
```css
footer a {
  color: var(--accent);
  text-decoration: none;
}

footer a:hover,
footer a:focus,
footer a:focus-visible {
  text-decoration: underline;
  outline: 2px solid var(--accent);
  outline-offset: 2px;
}
```

**TinyUSB WebUSB**:
```css
.command-history-entry:hover {
  background-color: #f0f0f0;
}

.command-history-entry:focus,
.command-history-entry:focus-visible {
  background-color: #f0f0f0;
  outline: 2px solid #0078d7;
  outline-offset: -2px;
}
```

For dark mode support:
```css
body.dark-mode .command-history-entry:focus,
body.dark-mode .command-history-entry:focus-visible {
  outline-color: #58a6ff;
}
```

## References
- [WCAG 2.1 - Focus Visible (2.4.7)](https://www.w3.org/WAI/WCAG21/Understanding/focus-visible.html)
- [MDN - :focus-visible pseudo-class](https://developer.mozilla.org/en-US/docs/Web/CSS/:focus-visible)
- [Web.dev - Focus visible](https://web.dev/focus-visible/)
