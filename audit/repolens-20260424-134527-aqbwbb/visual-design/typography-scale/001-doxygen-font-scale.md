---
title: "[MEDIUM] Inconsistent font-size scale in Doxygen documentation CSS"
severity: MEDIUM
domain: visual-design
lens: typography-scale
labels:
  - audit:visual-design/typography-scale
---

## Summary
The Doxygen documentation CSS (`doxygen_output/html/doxygen.css`) uses arbitrary and non-ratio-based font-size values without a coherent typographic scale. Found values include: 9px, 10px, 13px, 14px, 70%, 90%, 110%, 130%, 150%, 160%, 180%, and 10pt.

**Evidence locations:**
- `doxygen_output/html/doxygen.css:79-81` - Navigation font sizes: `--nav-font-size-level1: 13px`, `--nav-font-size-level2: 10px`, `--nav-font-size-level3: 9px`
- `doxygen_output/html/doxygen.css:154-156` - Body text: `font-size: 14px; line-height: 22px`
- `doxygen_output/html/doxygen.css` - Heading sizes using percentages: 160%, 150%, 130%, 180%
- `doxygen_output/html/doxygen.css` - Small text: 70%, 90%
- `doxygen_output/html/doxygen.css` - Mixed units: `font-size: 10pt; line-height: 125%`

## Impact
- **Maintainability**: Hard to adjust overall typography without hunting through scattered values
- **Visual consistency**: Arbitrary values (9px, 10px, 13px) don't follow a clear hierarchy
- **Accessibility**: Users relying on browser zoom may experience awkward scaling with mixed px/%/pt units
- **Design system**: No clear baseline (e.g., root `font-size: 16px` with `rem`-based scaling)

## Evidence
```css
/* Navigation - arbitrary px values */
--nav-font-size-level1: 13px;
--nav-font-size-level2: 10px;
--nav-font-size-level3: 9px;

/* Body text */
font-size: 14px;
line-height: 22px;

/* Headings - inconsistent percentage scale */
font-size: 160%;  /* h1? */
font-size: 150%;  /* h2? */
font-size: 130%;
font-size: 180%;

/* Small text */
font-size: 70%;
font-size: 90%;

/* Mixed unit */
font-size: 10pt;
line-height: 125%;
```

## Recommended Fix
Establish a coherent typographic scale using CSS custom properties:

1. **Define a root scale** (e.g., modular scale with 1.25 ratio):
```css
:root {
  --font-size-base: 16px;
  --font-scale: 1.25;
  --text-xs: calc(var(--font-size-base) / 1.563);  /* 10.24px */
  --text-sm: calc(var(--font-size-base) / 1.25);   /* 12.8px */
  --text-base: var(--font-size-base);              /* 16px */
  --text-lg: calc(var(--font-size-base) * var(--font-scale));     /* 20px */
  --text-xl: calc(var(--font-size-base) * 1.563);  /* 25px */
  --text-2xl: calc(var(--font-size-base) * 1.953); /* 31px */
}
```

2. **Replace px/%/pt values with scale tokens**:
```css
--nav-font-size-level1: var(--text-sm);
--nav-font-size-level2: var(--text-xs);
--nav-font-size-level3: var(--text-xs);
font-size: var(--text-base);
```

3. **Use unitless line-height** for better scalability:
```css
line-height: 1.5;  /* instead of 22px or 125% */
```

## References
- [Modular Scale - Type Scale Calculator](https://type-scale.com/)
- [CSS Tricks - Modular Scales](https://css-tricks.com/modular-scales-web/)
- [MDN - font-size](https://developer.mozilla.org/en-US/docs/Web/CSS/font-size)
- [WebAIM - Line Height](https://webaim.org/resources/guides/typography)
