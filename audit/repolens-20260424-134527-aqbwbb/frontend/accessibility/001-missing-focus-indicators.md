---
title: "[MEDIUM] Missing visible focus indicators for keyboard navigation"
severity: MEDIUM
domain: frontend
lens: a11y
labels:
  - "audit:frontend/accessibility"
---

## Summary
The web flasher interface lacks visible focus indicators for keyboard users. The main interactive element (`esp-web-install-button` with custom button styling) has `:hover` styles defined but no `:focus` or `:focus-visible` styles to indicate which element is currently focused.

**Location:** `web-flasher/index.html`, lines 165-175

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
  /* MISSING: :focus or :focus-visible style */
}

esp-web-install-button button:hover {
  background: var(--accent-hover);
  /* MISSING: :focus style */
}
```

## Impact
- **Keyboard users cannot see which element is focused** - Critical for users who navigate via Tab key
- **WCAG 2.1 Success Criterion 2.4.7 (Focus Visible)** - Level AA requirement not met
- Screen reader users and power users relying on keyboard navigation will have difficulty understanding the current focus position
- The `cursor: pointer` indicates interactivity but provides no visual feedback for focus state

## Evidence
- Line 165-169: Button styles without focus state
- Line 171-172: Only hover state defined, no focus state
- The button uses `border: none` which removes the default browser focus outline
- No custom focus style is provided as a replacement

## Recommended Fix
Add visible focus indicators for the install button:

```css
esp-web-install-button button:focus,
esp-web-install-button button:focus-visible {
  outline: 2px solid var(--accent-hover);
  outline-offset: 2px;
}

/* Optional: Enhance hover state */
esp-web-install-button button:hover {
  background: var(--accent-hover);
}
```

Alternatively, use a high-contrast focus ring:
```css
esp-web-install-button button:focus-visible {
  outline: 3px solid #fff;
  outline-offset: 2px;
}
```

## References
- [WCAG 2.1 Success Criterion 2.4.7 Focus Visible](https://www.w3.org/TR/WCAG21/#focus-visible)
- [MDN: :focus-visible pseudo-class](https://developer.mozilla.org/en-US/docs/Web/CSS/:focus-visible)
- [Inclusive Components: Focus](https://inclusive-components.design/focus/)
