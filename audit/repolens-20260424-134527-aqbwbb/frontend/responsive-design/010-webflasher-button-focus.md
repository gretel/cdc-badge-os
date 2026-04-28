---
title: "[LOW] Web flasher install button missing :focus state"
severity: LOW
domain: frontend
lens: responsive-design
labels:
  - responsive-design
  - accessibility
  - keyboard-navigation
---

## Summary
The web flasher's main install button (`web-flasher/index.html:158-172`) has `:hover` styling but lacks `:focus` and `:focus-visible` states for keyboard navigation.

**Location**: `web-flasher/index.html:158-172`

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

## Impact
**Minor accessibility impact**:
- Keyboard users (Tab key) get no visual feedback for focused button
- The `<esp-web-install-button>` is a custom web component that may not have built-in focus styling
- Screen reader users may not know which element is active
- Doesn't fully meet WCAG 2.1 Level AA focus visibility requirements

## Evidence
**Current state**: Only `:hover` is defined (line 170-172):
```css
esp-web-install-button button:hover {
  background: var(--accent-hover);
}
```

**Missing**: No `:focus`, `:focus-visible`, or `:active` states.

The button uses an external web component (`esp-web-install-button` from esp-web-tools), so focus styling may need to target the shadow DOM or the slotted button.

## Recommended Fix
Add focus states for keyboard navigation:

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

esp-web-install-button button:hover,
esp-web-install-button button:focus,
esp-web-install-button button:focus-visible {
  background: var(--accent-hover);
  outline: 2px solid var(--accent);
  outline-offset: 2px;
}

esp-web-install-button button:active {
  background: var(--accent-hover);
  transform: scale(0.98);
}
```

**Note**: Since `esp-web-install-button` is a web component, the button is slotted. If the above doesn't work, you may need to:
1. Style the slotted button via CSS `::part()` if the component supports it
2. Add focus styles to the web component wrapper itself
3. Check if the component has built-in focus handling

## References
- [WCAG 2.1 - Focus Visible (2.4.7)](https://www.w3.org/WAI/WCAG21/Understanding/focus-visible.html)
- [MDN - :focus-visible pseudo-class](https://developer.mozilla.org/en-US/docs/Web/CSS/:focus-visible)
- [Web Components - Part pseudo-elements](https://developer.mozilla.org/en-US/docs/Web/CSS/::part)
- [Web.dev - Focus visible](https://web.dev/focus-visible/)
