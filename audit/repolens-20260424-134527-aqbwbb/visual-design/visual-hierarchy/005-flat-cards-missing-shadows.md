---
title: "[LOW] Flat card patterns missing shadows - visually indistinguishable from background"
severity: LOW
domain: visual-design
lens: visual-hierarchy
labels:
  - "audit:visual-design/visual-hierarchy"
---

## Summary
The web-flasher `index.html` uses card patterns with borders but no box-shadow, making them visually flat and potentially indistinguishable from the background at certain contrast levels. Cards use `border: 1px solid var(--border)` but lack elevation shadows.

**Affected elements:**
- Line 76-78: `.card` class - border but no shadow
- Line 226-228: Info card with image
- Line 237: Steps card
- Line 261: Flash section card
- Line 277: Requirements card

## Impact
1. **Visual grouping unclear**: Cards rely solely on border and background color for separation, which may break at different contrast settings.
2. **Flat hierarchy**: Without shadows, cards don't appear to "float" above the background, reducing perceived depth.
3. **Accessibility**: Users with low vision may have difficulty distinguishing card boundaries.

## Evidence
```css
/* web-flasher/index.html:76-78 */
.card {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 0.75rem;
    padding: 1.5rem;
    margin-bottom: 1.25rem;
    /* No box-shadow - flat appearance */
}
```

## Recommended Fix
1. **Add subtle elevation shadows** to cards:
```css
.card {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 0.75rem;
    padding: 1.5rem;
    margin-bottom: 1.25rem;
    box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);  /* Subtle elevation */
}
```

2. **Add hover elevation** for interactive cards:
```css
.card:hover {
    box-shadow: 0 4px 8px rgba(0, 0, 0, 0.15);
}
```

3. **Consider border removal** if shadow provides sufficient separation:
```css
.card {
    background: var(--surface);
    border: none;  /* Let shadow provide separation */
    border-radius: 0.75rem;
    padding: 1.5rem;
    box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
}
```

## References
- [Material Design: Elevation](https://m3.material.io/styles/elevation)
- [MDN: box-shadow](https://developer.mozilla.org/en-US/docs/Web/CSS/box-shadow)
- [W3C: Visual separation](https://www.w3.org/WAI/WCAG21/Understanding/contrast-minimum.html)
