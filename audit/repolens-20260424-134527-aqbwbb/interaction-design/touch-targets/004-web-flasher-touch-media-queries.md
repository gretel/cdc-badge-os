---
title: "[LOW] Web flasher lacks touch-specific media queries"
severity: LOW
domain: web-flasher
lens: touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The web flasher (`web-flasher/index.html`) uses hover styles but lacks `@media (hover: none)` or `@media (pointer: coarse)` queries to optimize the interface for touch devices.

**Location:** `web-flasher/index.html:170, 208`

## Impact
- **Mobile UX:** No touch-specific optimizations for devices without a mouse
- **Hover-dependent elements:** If any UI relies on hover for information, it may not be accessible on touch devices
- **Suboptimal sizing:** Touch devices could benefit from larger tap targets and adjusted spacing

## Evidence
The web flasher has hover styles:

1. **Main button hover** (line 170):
```css
esp-web-install-button button:hover {
  background: var(--accent-hover);
}
```

2. **Footer link hover** (line 208):
```css
footer a:hover {
  text-decoration: underline;
}
```

No touch-capability queries found:
```
grep "@media (hover: none)\|@media (pointer: coarse)" → no results
```

The main install button has good padding (0.85rem × 2rem ≈ 13.6px × 32px), but could be larger on touch devices.

## Recommended Fix
Add touch-specific media queries to optimize for mobile devices:

```css
@media (any-pointer: coarse) {
  /* Increase button size for better touch accuracy */
  esp-web-install-button button {
    padding: 1rem 2.5rem;
    min-height: 48px;
  }

  /* Increase footer link tap area */
  footer a {
    padding: 8px 12px;
    display: inline-block;
  }

  /* Increase step list spacing for easier tapping */
  .steps li {
    padding: 1rem 1rem 1rem 3.5rem;
  }

  /* Add touch-action for faster tap response */
  button, a {
    touch-action: manipulation;
  }
}

@media (hover: none) {
  /* Underline links by default on touch devices */
  footer a {
    text-decoration: underline;
  }

  /* Make hover states more obvious */
  esp-web-install-button button {
    opacity: 0.9;
  }
}
```

## References
- [CSS Media Queries: Hover](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/hover)
- [CSS Media Queries: Pointer](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/any-pointer)
- [Material Design Touch Targets](https://m3.material.io/foundations/interactions/touch-interaction/touch-targets)
