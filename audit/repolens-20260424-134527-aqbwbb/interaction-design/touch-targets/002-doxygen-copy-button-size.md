---
title: "[LOW] Doxygen docs copy button undersized for touch (28x28px)"
severity: LOW
domain: documentation
lens: touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The "copy code snippet" button in the Doxygen documentation (`third_party/libtropic/docs/doxygen/html/doxygen-awesome.css`) is only 28x28px, below the recommended 44x44px minimum touch target size for mobile devices.

**Location:** `third_party/libtropic/docs/doxygen/html/doxygen-awesome.css:2408-2422`

## Impact
- **Mobile Usability:** Small 28x28px button is difficult to tap accurately on touch devices
- **Accessibility:** May not meet WCAG 2.5.8 minimum touch target guidelines (44x44px)
- **User Experience:** Higher rate of missed taps, especially for users with larger fingers or motor control challenges

## Evidence
The copy button styles in `doxygen-awesome.css` (lines 2408-2422):

```css
doxygen-awesome-fragment-copy-button {
    opacity: 0;
    background: var(--fragment-background);
    width: 28px;
    height: 28px;
    position: absolute;
    right: calc(var(--spacing-large) - (var(--spacing-large) / 2.5));
    top: calc(var(--spacing-large) - (var(--spacing-large) / 2.5));
    border: 1px solid var(--fragment-foreground);
    cursor: pointer;
    border-radius: var(--border-radius-small);
    display: flex;
    justify-content: center;
    align-items: center;
}
```

The button is 28x28px with an 18x18px SVG icon inside. The 28px dimension is below the 44px WCAG recommendation.

## Recommended Fix
Increase the button size to at least 36x36px (minimum practical) or 44x44px (WCAG compliant):

```css
doxygen-awesome-fragment-copy-button {
    opacity: 0;
    background: var(--fragment-background);
    width: 36px;  /* Changed from 28px */
    height: 36px; /* Changed from 28px */
    position: absolute;
    right: calc(var(--spacing-large) - (var(--spacing-large) / 2.5));
    top: calc(var(--spacing-large) - (var(--spacing-large) / 2.5));
    border: 1px solid var(--fragment-foreground);
    cursor: pointer;
    border-radius: var(--border-radius-small);
    display: flex;
    justify-content: center;
    align-items: center;
}
```

Optionally, add `touch-action: manipulation` for faster tap response:

```css
doxygen-awesome-fragment-copy-button {
    /* ... existing styles ... */
    touch-action: manipulation;
}
```

**Note:** This is a third-party Doxygen theme (doxygen-awesome-css). The fix may need to be applied via custom CSS override or by contributing to the upstream project.

## References
- [WCAG 2.5.8 Target Size (Minimum)](https://www.w3.org/WAI/WCAG21/Understanding/target-size.html)
- [Material Design Touch Targets](https://m3.material.io/foundations/interactions/touch-interaction/touch-targets)
- [Doxygen Awesome CSS GitHub](https://github.com/jothepro/doxygen-awesome-css)
