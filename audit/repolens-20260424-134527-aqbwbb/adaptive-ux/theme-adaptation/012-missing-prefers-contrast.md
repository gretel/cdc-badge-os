---
title: "[MEDIUM] Web flasher missing `prefers-contrast` support for high-contrast mode"
severity: MEDIUM
domain: adaptive-ux/theme-adaptation
lens: theme-adaptation
labels:
  - accessibility
  - contrast
  - high-contrast
---

## Summary
The web flasher (`web-flasher/index.html`) does not include any `prefers-contrast: more` media query support. While doxygen documentation coverage is tracked separately (see related issue #4), the web flasher is a critical user-facing interface that needs high-contrast support for users who enable it in their OS.

## Impact
Users with visual impairments who rely on high-contrast mode will experience:
- Low-contrast color combinations (`--text-muted: #8b949e` on `--bg: #0d1117`)
- Subtle borders (`--border: #30363d`) that may blend together
- Focus rings that are not visible or use insufficient contrast
- Step indicators and notices that may be hard to distinguish

## Evidence
**File: `web-flasher/index.html`**

Lines 12-22 define CSS custom properties but no high-contrast overrides:
```css
:root {
  --bg: #0d1117;
  --surface: #161b22;
  --border: #30363d;
  --text: #e6edf3;
  --text-muted: #8b949e;
  --accent: #58a6ff;
  --accent-hover: #79c0ff;
  --warning: #d29922;
  --step-bg: #1c2128;
}
```

Lines 158-168 define button styles without high-contrast focus states:
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
```

No `@media (prefers-contrast: more)` queries exist in the file.

## Recommended Fix
Add `prefers-contrast: more` overrides to the web flasher stylesheet:

```css
@media (prefers-contrast: more) {
  :root {
    --bg: #000000;
    --surface: #0a0a0a;
    --border: #ffffff;
    --text: #ffffff;
    --text-muted: #e0e0e0;
    --accent: #4da3ff;
    --accent-hover: #66b3ff;
    --warning: #ffcc00;
    --step-bg: #111111;
  }
  
  /* Increase border widths for better visibility */
  .card, .steps li, .notice {
    border-width: 2px;
  }
  
  /* Stronger focus rings */
  esp-web-install-button button:focus-visible {
    outline: 3px solid var(--accent);
    outline-offset: 2px;
  }
  
  .steps li:focus-visible {
    outline: 2px solid var(--accent);
    outline-offset: 2px;
  }
  
  /* Ensure notice icon is visible */
  .notice-icon {
    font-weight: bold;
  }
}
```

**Related issues:** Doxygen documentation `prefers-contrast` support is tracked in issue #4.

## References
- [MDN: prefers-contrast](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/prefers-contrast)
- [WCAG 2.1 Criterion 1.4.6: Contrast (Enhanced)](https://www.w3.org/TR/WCAG21/#contrast-enhanced)
- [Web.dev: Create a high-contrast experience](https://web.dev/high-contrast/)
- [Inclusive Components: High Contrast Mode](https://inclusive-components.design/high-contrast-mode/)

</content>