---
title: "[LOW] Inconsistent font-weight usage in Doxygen documentation CSS"
severity: LOW
domain: visual-design
lens: typography-scale
labels:
  - audit:visual-design/typography-scale
---

## Summary
The Doxygen documentation CSS (`doxygen_output/html/doxygen.css`) uses font-weight values inconsistently, mixing numeric values (400, 700) with keyword values (bold, normal) without a clear pattern.

**Evidence locations:** `doxygen_output/html/doxygen.css`

## Impact
- **Maintainability**: Hard to understand the intended weight hierarchy
- **Visual consistency**: Same semantic weight may be rendered differently
- **Code readability**: Mixed styles make CSS harder to scan

## Evidence
```css
/* Numeric weights */
font-weight: 400;
font-weight: 700;

/* Keyword weights */
font-weight: normal;
font-weight: bold;

/* Usage pattern unclear - both 400 and normal used, both 700 and bold used */
```

## Recommended Fix
Standardize on one notation style:

**Option 1 - Use numeric values:**
```css
font-weight: 400;  /* normal */
font-weight: 500;  /* medium */
font-weight: 600;  /* semibold */
font-weight: 700;  /* bold */
```

**Option 2 - Use keywords:**
```css
font-weight: normal;
font-weight: bold;
```

Recommend numeric values for better granularity if the font supports it.

## References
- [MDN - font-weight](https://developer.mozilla.org/en-US/docs/Web/CSS/font-weight)
- [CSS Tricks - Font Weight](https://css-tricks.com/almanac/properties/f/font-weight/)
