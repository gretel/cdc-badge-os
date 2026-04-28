---
title: "[MEDIUM] Main install button missing focus, active, and disabled states"
severity: MEDIUM
domain: web-flasher
lens: interactive-feedback
labels:
  - "focus-state"
  - "active-state"
  - "disabled-state"
---

## Summary
The main install button (`esp-web-install-button button` in `web-flasher/index.html:168-172`) has only a `:hover` state defined. It lacks `:active`, `:focus`, `:focus-visible`, and `:disabled` states.

## Impact
- **Keyboard accessibility**: Users navigating via keyboard receive no visual indication when the button receives focus
- **Press feedback**: Users don't get tactile visual feedback when clicking/pressing the button (between mousedown and mouseup)
- **Disabled clarity**: When the button becomes disabled (e.g., during flashing), there's no visual cue to indicate the state change

## Evidence
File: `web-flasher/index.html`, lines 165-172

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

**Missing states:**
- `:focus` / `:focus-visible` - no outline or ring for keyboard navigation
- `:active` - no visual change on mousedown
- `:disabled` - no styling for when button is disabled

## Recommended Fix
Add the following CSS rules after the existing `:hover` state:

```css
esp-web-install-button button:focus-visible {
  outline: 2px solid var(--accent);
  outline-offset: 2px;
}

esp-web-install-button button:active {
  background: var(--accent-hover);
  transform: scale(0.98);
}

esp-web-install-button button:disabled {
  background: var(--text-muted);
  cursor: not-allowed;
  opacity: 0.6;
}
```

## References
- [MDN: Focus states](https://developer.mozilla.org/en-US/docs/Web/CSS/:focus-visible)
- [MDN: Active state](https://developer.mozilla.org/en-US/docs/Web/CSS/:active)
- [WAI-ARIA Authoring Practices - Button](https://www.w3.org/WAI/ARIA/apg/patterns/button/)
