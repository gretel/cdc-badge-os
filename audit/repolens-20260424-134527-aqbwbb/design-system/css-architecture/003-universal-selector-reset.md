---
title: "[LOW] Global selector `*` resets all elements with limited specificity control"
severity: LOW
domain: design-system
lens: css-architecture
labels:
  - "specificity"
  - "reset"
---

## Summary
The web-flasher uses a universal selector reset at line 24:
```css
* { margin: 0; padding: 0; box-sizing: border-box; }
```

This applies to ALL elements including web component shadow DOM (`esp-web-install-button`) and third-party elements, which may cause unintended side effects.

## Impact
- **Third-party components**: The `esp-web-install-button` web component's internal styles may be affected
- **Specificity**: Hard to override since `*` matches everything
- **Maintainability**: Need to use more specific selectors to restore margins/padding where needed

## Evidence
```css
/* Line 24 */
* { margin: 0; padding: 0; box-sizing: border-box; }

/* Line 162-169: Need to override box-sizing for web component button */
esp-web-install-button button {
  background: var(--accent);
  color: var(--bg);
  border: none;
  border-radius: 0.5rem;
  padding: 0.85rem 2rem;  /* Re-adding padding after reset */
  ...
}
```

## Recommended Fix
Use a more targeted reset approach:

**Option 1**: Box-sizing reset only (modern approach):
```css
*,
*::before,
*::after {
  box-sizing: border-box;
}

/* Then explicitly reset only what you need */
body {
  margin: 0;
  padding: 0;
}
```

**Option 2**: Use a modern reset like `reset.css` or `modern-normalize` for better third-party compatibility.

## References
- [CSS Reset - Modern approach](https://www.joshwcomeau.com/css/custom-css-reset/)
- [Box-sizing reset pattern](https://css-tricks.com/box-sizing/)
- [Modern Normalize](https://github.com/sindresorhus/modern-normalize)
