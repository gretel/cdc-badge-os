---
title: "[LOW] Libtropic doxygen dark mode toggle is undersized for touch"
severity: LOW
domain: documentation
lens: touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The dark mode toggle button in the Libtropic Doxygen documentation (`third_party/libtropic/docs/doxygen/html/doxygen-awesome.css`) has a fixed size of 33px (from `--searchbar-height`), which is below the recommended 44px minimum for comfortable touch targets.

**Location:** `third_party/libtropic/docs/doxygen/html/doxygen-awesome.css:135, 252-272`

## Impact
- **Mobile Usability:** 33px button height is below the 44px recommended minimum for touch targets
- **Accessibility:** Does not meet WCAG 2.5.8 Target Size (Minimum) guidelines
- **User Experience:** Small tap area may lead to selection errors on touch devices

## Evidence
The dark mode toggle styles in `doxygen-awesome.css`:

```css
/* Line 135 - CSS variable definition */
--searchbar-height: 33px;

/* Lines 252-272 - Dark mode toggle element */
doxygen-awesome-dark-mode-toggle {
    display: inline-block;
    margin: 0 0 0 var(--spacing-small);
    padding: 0;
    width: var(--searchbar-height);      /* 33px */
    height: var(--searchbar-height);     /* 33px */
    background: none;
    border: none;
    border-radius: var(--searchbar-height);
    vertical-align: middle;
    text-align: center;
    line-height: var(--searchbar-height);
    font-size: 22px;
    display: flex;
    align-items: center;
    justify-content: center;
    user-select: none;
    cursor: pointer;
}
```

The toggle button is constrained to 33px × 33px, which is approximately 25% smaller than the recommended 44px minimum touch target size.

## Recommended Fix
Increase the touch target size for the dark mode toggle, especially for coarse pointers:

```css
/* Base size - increase from 33px */
doxygen-awesome-dark-mode-toggle {
    display: inline-block;
    margin: 0 0 0 var(--spacing-small);
    padding: 0;
    width: 40px;  /* Increased from var(--searchbar-height) */
    height: 40px; /* Increased from var(--searchbar-height) */
    background: none;
    border: none;
    border-radius: 50%;
    vertical-align: middle;
    text-align: center;
    line-height: 40px;
    font-size: 22px;
    display: flex;
    align-items: center;
    justify-content: center;
    user-select: none;
    cursor: pointer;
    /* Add padding for larger touch area */
    padding: 4px;
}

/* For coarse pointers, increase further */
@media (any-pointer: coarse) {
    doxygen-awesome-dark-mode-toggle {
        width: 48px;
        height: 48px;
        line-height: 48px;
        font-size: 26px;
    }
}
```

Alternatively, define a new CSS variable specifically for touch-friendly sizes:

```css
:root {
    --searchbar-height: 33px;
    --touch-target-size: 44px;  /* New variable */
}

doxygen-awesome-dark-mode-toggle {
    width: var(--touch-target-size);
    height: var(--touch-target-size);
    line-height: var(--touch-target-size);
}

@media (any-pointer: coarse) {
    :root {
        --touch-target-size: 48px;
    }
}
```

## References
- [WCAG 2.5.8 Target Size (Minimum)](https://www.w3.org/WAI/WCAG21/Understanding/target-size.html)
- [Material Design Touch Targets](https://m3.material.io/foundations/interactions/touch-interaction/touch-targets)
- [MDN: CSS Media Queries - Pointer](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/any-pointer)
