---
title: "[LOW] Footer link missing focus, active, and visited states"
severity: LOW
domain: web-flasher
lens: interactive-feedback
labels:
  - "focus-state"
  - "active-state"
  - "visited-state"
---

## Summary
The footer link (`footer a` in `web-flasher/index.html:203-207`) has only a `:hover` state with underline. It lacks `:focus`, `:focus-visible`, `:active`, and `:visited` states.

## Impact
- **Keyboard accessibility**: Keyboard users get no focus indicator on the link
- **Navigation feedback**: No visual feedback when the link is pressed (mousedown)
- **Visited state**: Users cannot tell if they've already visited the GitHub link

## Evidence
File: `web-flasher/index.html`, lines 203-207

```css
footer a {
  color: var(--accent);
  text-decoration: none;
}

footer a:hover {
  text-decoration: underline;
}
```

**Missing states:**
- `:focus` / `:focus-visible` - no outline for keyboard navigation
- `:active` - no color change on mousedown
- `:visited` - no differentiation for visited links

## Recommended Fix
Add the following CSS rules after the existing `:hover` state:

```css
footer a:focus-visible {
  outline: 2px solid var(--accent);
  outline-offset: 2px;
}

footer a:active {
  color: var(--accent-hover);
}

footer a:visited {
  color: var(--accent); /* or a slightly different shade if desired */
}
```

## References
- [MDN: Focus states](https://developer.mozilla.org/en-US/docs/Web/CSS/:focus-visible)
- [MDN: Visited state](https://developer.mozilla.org/en-US/docs/Web/CSS/:visited)
- [WAI-ARIA Authoring Practices - Link](https://www.w3.org/WAI/ARIA/apg/patterns/link/)
