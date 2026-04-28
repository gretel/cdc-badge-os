---
title: "[HIGH] Link "Hardware details on GitHub" lacks underline for non-color indication"
severity: HIGH
domain: cdc-badge-os
lens: session-lighthouse
labels:
  - "audit:toolgate/session-lighthouse"
---

## Summary
The link at line 234 uses only color change (`--accent: #58a6ff`) to indicate it's a link, with `text-decoration: none`. This fails WCAG 2.1 AA requirements for non-color visual indicators.

**File:** `web-flasher/index.html` (line 234)

## Impact
- **Accessibility**: Users with color blindness may not identify the link
- **Lighthouse score**: Will flag "link-name" and "color-contrast" audits
- **WCAG compliance**: Fails Success Criterion 1.4.1 (Use of Color)

## Evidence
Current CSS (lines 204-208):
```css
footer a {
  color: var(--accent);
  text-decoration: none;
}

footer a:hover {
  text-decoration: underline;
}
```

Current HTML (line 234):
```html
<a href="https://github.com/riatlabs/cdc-badge" style="color: var(--accent); text-decoration: none;">Hardware details on GitHub</a>
```

The link only uses color (#58a6ff blue) to distinguish itself. On hover, underline appears, but initial state has no visual indicator beyond color.

## Recommended Fix
Add a non-color visual indicator:

**Option 1: Always show underline:**
```css
footer a {
  color: var(--accent);
  text-decoration: underline;
}

footer a:hover {
  text-decoration: none;
}
```

**Option 2: Use border or other indicator:**
```css
footer a {
  color: var(--accent);
  border-bottom: 1px solid var(--accent);
}

footer a:hover {
  border-bottom-color: transparent;
}
```

**Option 3: Add underline with slight offset:**
```css
footer a {
  color: var(--accent);
  text-underline-offset: 3px;
  text-decoration: underline;
  text-decoration-thickness: 1px;
}
```

For the inline style at line 234, remove `text-decoration: none`:
```html
<a href="https://github.com/riatlabs/cdc-badge" style="color: var(--accent);">Hardware details on GitHub</a>
```

## References
- [WCAG 1.4.1 Use of Color](https://www.w3.org/WAI/WCAG21/Understanding/use-of-color.html)
- [Lighthouse link-name audit](https://web.dev/link-href/)
- [MDN text-decoration](https://developer.mozilla.org/en-US/docs/Web/CSS/text-decoration)
