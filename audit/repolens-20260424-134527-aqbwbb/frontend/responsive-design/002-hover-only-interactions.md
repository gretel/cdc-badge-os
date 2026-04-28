---
title: "[LOW] Hover-only visual feedback on footer link"
severity: LOW
domain: frontend
lens: responsive-design
labels:
  - responsive-design
  - touch-interactions
  - accessibility
---

## Summary
The web flasher footer link (`web-flasher/index.html:208`) only provides visual feedback on hover, with no touch-state indication for mobile/tablet users:

```css
footer a:hover {
  text-decoration: underline;
}
```

## Impact
**Minor usability impact** on touch devices:
- Touch users don't get clear visual feedback when tapping the footer link
- Hover state may persist unexpectedly on some touch devices after tap
- Less obvious clickable target on touch screens

## Evidence
**Location**: `web-flasher/index.html:195-210`

```css
footer a {
  color: var(--accent);
  text-decoration: none;
}

footer a:hover {
  text-decoration: underline;
}
```

The link uses `text-decoration: none` by default and only shows underline on hover. Touch devices typically don't trigger `:hover` on first tap (some show it on second tap).

## Recommended Fix
Add `:focus` and `:active` states for better touch/keyboard accessibility:

```css
footer a {
  color: var(--accent);
  text-decoration: none;
}

footer a:hover,
footer a:focus,
footer a:active {
  text-decoration: underline;
}
```

Alternatively, make the underline always visible for better touch target identification:

```css
footer a {
  color: var(--accent);
  text-decoration: underline;
}
```

## References
- [WCAG 2.1 - Focus Visible](https://www.w3.org/WAI/WCAG21/Understanding/focus-visible.html)
- [MDN - CSS Pseudo-classes](https://developer.mozilla.org/en-US/docs/Web/CSS/Pseudo-classes)
- [Web.dev - Focus visible](https://web.dev/focus-visible/)
