---
title: "[LOW] Inline GitHub link missing all interaction states"
severity: LOW
domain: web-flasher
lens: interactive-feedback
labels:
  - "hover-state"
  - "focus-state"
  - "active-state"
  - "visited-state"
---

## Summary
The inline link in the info card (`web-flasher/index.html:239`) has `text-decoration: none` inline style but only inherits default browser behavior for interaction states. It lacks explicit `:hover`, `:focus`, `:active`, and `:visited` states.

## Impact
- **Discoverability**: Users may not realize the text is clickable without hover
- **Keyboard accessibility**: No visible focus indicator for keyboard users
- **Press feedback**: No visual change when the link is pressed
- **Visited state**: No indication if the link has been visited

## Evidence
File: `web-flasher/index.html`, line 239

```html
<a href="https://github.com/riatlabs/cdc-badge" style="color: var(--accent); text-decoration: none;">Hardware details on GitHub</a>
```

The link has inline styles but no CSS rules for interaction states. It only has:
- Default color: `var(--accent)`
- No underline by default (`text-decoration: none`)

**Missing states:**
- `:hover` - no underline or color change on hover
- `:focus` / `:focus-visible` - no outline for keyboard navigation
- `:active` - no color change on mousedown
- `:visited` - no differentiation for visited links

## Recommended Fix
Add a CSS class for inline links and apply it to the link:

```css
/* Add to style block */
.link-inline {
  color: var(--accent);
  text-decoration: none;
}

.link-inline:hover {
  text-decoration: underline;
}

.link-inline:focus-visible {
  outline: 2px solid var(--accent);
  outline-offset: 2px;
}

.link-inline:active {
  color: var(--accent-hover);
}

.link-inline:visited {
  color: var(--accent);
}
```

Then update the HTML:
```html
<a href="https://github.com/riatlabs/cdc-badge" class="link-inline">Hardware details on GitHub</a>
```

## References
- [MDN: Link states](https://developer.mozilla.org/en-US/docs/Web/CSS/Link_states)
- [WAI-ARIA Authoring Practices - Link](https://www.w3.org/WAI/ARIA/apg/patterns/link/)
