---
title: "[MEDIUM] Missing and inconsistent line-height in Doxygen documentation CSS"
severity: MEDIUM
domain: visual-design
lens: typography-scale
labels:
  - audit:visual-design/typography-scale
---

## Summary
The Doxygen documentation CSS (`doxygen_output/html/doxygen.css`) has inconsistent line-height values, with some elements missing explicit line-height declarations entirely, relying on browser defaults.

**Evidence locations:**
- `doxygen_output/html/doxygen.css` - Body text: `line-height: 22px` (fixed, doesn't scale with font-size)
- `doxygen_output/html/doxygen.css` - Some headings have `line-height: 1.15em`
- `doxygen_output/html/doxygen.css` - Code blocks: `line-height: 125%`
- `doxygen_output/html/doxygen.css` - Many elements have no line-height at all

## Impact
- **Readability**: Missing line-height on body text or headings creates cramped text (default ~1.2)
- **Inconsistency**: Mix of fixed `px`, relative `em`, and percentage values
- **Scalability**: Fixed `line-height: 22px` doesn't scale proportionally when font-size changes
- **Accessibility**: Tight line-height (below 1.2 for body) reduces readability for users with visual impairments

## Evidence
```css
/* Fixed pixel line-height - doesn't scale */
font-size: 14px;
line-height: 22px;  /* Ratio: ~1.57, but fixed */

/* Heading with em-based line-height */
line-height: 1.15em;  /* Too tight for larger headings */

/* Code block with percentage */
font-size: 10pt;
line-height: 125%;  /* 1.25 ratio */

/* Many elements missing line-height entirely */
font-size: 13px;
font-family: var(--font-family-nav);
/* No line-height specified - browser default applies */
```

## Recommended Fix
Use unitless line-height values for consistent, scalable typography:

1. **Define line-height variables**:
```css
:root {
  --line-height-tight: 1.25;
  --line-height-normal: 1.5;
  --line-height-loose: 1.75;
}
```

2. **Apply to elements**:
```css
/* Body text - unitless for scalability */
font-size: var(--text-base);
line-height: var(--line-height-normal);  /* 1.5 */

/* Headings - tighter but still readable */
font-size: var(--text-xl);
line-height: var(--line-height-tight);  /* 1.25 */

/* Code blocks */
font-size: var(--text-sm);
line-height: var(--line-height-normal);  /* 1.5 */
```

3. **Recommended ratios** (per web typography best practices):
- Body text: 1.45 - 1.6
- Headings: 1.1 - 1.3 (tighter due to larger size)
- Small text: 1.5 - 1.8 (looser for readability)

## References
- [WebAIM - Line Height](https://webaim.org/resources/guides/typography#line-height)
- [MDN - line-height](https://developer.mozilla.org/en-US/docs/Web/CSS/line-height)
- [CSS Tricks - Perfect Type](https://css-tricks.com/examples/PerfectType/)
- [Tachyons - Typography Scale](https://tachyons.io/docs/typography/measure/)
