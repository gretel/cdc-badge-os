---
title: "[MEDIUM] Inconsistent emphasis techniques - color and bold applied simultaneously without clear hierarchy"
severity: MEDIUM
domain: visual-design
lens: visual-hierarchy
labels:
  - "audit:visual-design/visual-hierarchy"
---

## Summary
The web-flasher `index.html` applies multiple emphasis techniques (color, font-weight, font-size) to the same elements without a clear primary focal point. This creates visual noise and competing emphasis patterns:

**Inconsistent patterns found:**
- Line 230: `<strong style="color: var(--text);">` - bold + color override
- Line 117: `font-weight: 700` (step numbers) with `color: var(--bg)` on accent background
- Line 121: `color: var(--accent)` for step labels
- Lines 50, 84, 165: `font-weight: 600` used in multiple contexts (header h1, card h2, button)
- Line 117: `font-weight: 700` only for step numbers

**Competing emphasis:**
- Step numbers (line 117) use white on accent background
- Step labels (line 121) use accent color
- Card titles (line 84) use 600 weight
- Main title (line 50) uses 600 weight but larger size

## Impact
1. **Visual noise**: When everything is emphasized with different techniques, nothing stands out as most important.
2. **Inconsistent patterns**: Some cards bold the title, others use color, others rely on size alone.
3. **Over-emphasis**: Using both bold AND color simultaneously reduces the impact of each technique.
4. **Accessibility**: Color is used as the sole differentiator in some places (line 121 accent color) without weight or size reinforcement.

## Evidence
```css
/* web-flasher/index.html:49-50 */
header h1 {
    font-size: 1.75rem;
    font-weight: 600;  /* Bold for main title */
}

/* web-flasher/index.html:83-84 */
.card h2 {
    font-size: 1.1rem;
    font-weight: 600;  /* Same weight as h1, different size */
}

/* web-flasher/index.html:116-117 */
.steps li::before {
    font-size: 0.8rem;
    font-weight: 700;  /* Heavier than headings - competing emphasis */
}

/* web-flasher/index.html:121 */
.steps li strong {
    color: var(--accent);  /* Color emphasis without weight change */
}
```

## Recommended Fix
1. **Establish a clear emphasis hierarchy**:
   - Primary emphasis: Size + weight (main title)
   - Secondary emphasis: Weight only (section headings)
   - Tertiary emphasis: Color only (links, accents)

2. **Consolidate emphasis techniques** - use one primary technique per context:
```css
/* Main title - size is primary */
header h1 {
    font-size: 1.75rem;
    font-weight: 600;
}

/* Step numbers - use weight consistently */
.steps li::before {
    font-weight: 600;  /* Match heading weight, not heavier */
}

/* Step labels - use color without extra bold */
.steps li strong {
    color: var(--accent);
    font-weight: 600;  /* Inherit base weight, not extra bold */
}
```

3. **Create emphasis utility classes** for consistency:
```css
.text-primary { font-size: 1.1rem; font-weight: 600; }
.text-secondary { font-weight: 500; }
.text-accent { color: var(--accent); }
```

## References
- [MDN: font-weight](https://developer.mozilla.org/en-US/docs/Web/CSS/font-weight)
- [Material Design: Emphasis](https://m3.material.io/styles/typography/tokens#weight)
- [A11y: Color contrast](https://www.w3.org/WAI/WCAG21/Understanding/contrast-minimum.html)
