---
title: "[MEDIUM] Links missing visible focus indicators for keyboard navigation"
severity: MEDIUM
domain: frontend
lens: a11y
labels:
  - "audit:frontend/accessibility"
---

## Summary
Links in the web flasher interface have hover states but lack dedicated focus states for keyboard users. Two links are affected:
1. Footer link (line 203-210) has `:hover` but no `:focus` or `:focus-visible`
2. Inline style link on line 234 has `text-decoration: none` with only hover recovery

**Location:** `web-flasher/index.html`

### Footer link styles (lines 203-210):
```css
footer a {
  color: var(--accent);
  text-decoration: none;
  /* MISSING: focus state */
}

footer a:hover {
  text-decoration: underline;
  /* MISSING: :focus or :focus-visible style */
}
```

### Inline link (line 234):
```html
<a href="https://github.com/riatlabs/cdc-badge" style="color: var(--accent); text-decoration: none;">Hardware details on GitHub</a>
```

## Impact
- **Keyboard users cannot see focused links** - Critical for users navigating via Tab key
- **WCAG 2.1 Success Criterion 2.4.7 (Focus Visible)** - Level AA requirement not met
- **Screen reader users miss context** - Cannot identify which link is currently selected
- The `text-decoration: none` removes the browser's default visual indicator, and no custom focus style replaces it
- Users with motor impairments who rely on keyboard navigation will have difficulty

## Evidence
- Line 203-206: Footer link styles with `text-decoration: none`
- Line 208-210: Only `:hover` state defined, no `:focus` or `:focus-visible`
- Line 234: Inline style link with `text-decoration: none` and no focus recovery
- No focus styles exist for any `<a>` elements in the stylesheet

## Recommended Fix
Add focus indicators for all links:

### For footer links (lines 203-210):
```css
footer a {
  color: var(--accent);
  text-decoration: none;
}

footer a:hover {
  text-decoration: underline;
}

footer a:focus,
footer a:focus-visible {
  outline: 2px solid var(--accent-hover);
  outline-offset: 2px;
  text-decoration: underline;
}
```

### For inline style link (line 234):
Option 1 - Move to CSS class:
```css
.github-link {
  color: var(--accent);
  text-decoration: none;
}

.github-link:hover,
.github-link:focus {
  text-decoration: underline;
}
```

Then update HTML:
```html
<a href="https://github.com/riatlabs/cdc-badge" class="github-link">Hardware details on GitHub</a>
```

Option 2 - Add inline focus style:
```html
<a href="https://github.com/riatlabs/cdc-badge" style="color: var(--accent); text-decoration: none;" onfocus="this.style.textDecoration='underline'" onblur="this.style.textDecoration='none'">Hardware details on GitHub</a>
```

### Best practice - Global link focus styles:
```css
a:focus,
a:focus-visible {
  outline: 2px solid var(--accent-hover);
  outline-offset: 2px;
  text-decoration: underline;
}
```

## References
- [WCAG 2.1 Success Criterion 2.4.7 Focus Visible](https://www.w3.org/TR/WCAG21/#focus-visible)
- [MDN: :focus-visible pseudo-class](https://developer.mozilla.org/en-US/docs/Web/CSS/:focus-visible)
- [Inclusive Components: Menus and Menubars - Focus](https://inclusive-components.design/menus-button/)

</content>