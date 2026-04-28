---
title: "[MEDIUM] Google Fonts import missing font-display strategy"
severity: MEDIUM
domain: visual-design
lens: typography-scale
labels:
  - audit:visual-design/typography-scale
---

## Summary
The Google Fonts import in `doxygen-awesome.css` uses `display=swap` but does not explicitly define a `font-display` strategy in the CSS. While Google Fonts handles this server-side, the CSS could benefit from a more explicit strategy or preload hint for better performance and reduced layout shift.

**Evidence location:** `third_party/libtropic/docs/doxygen/html/doxygen-awesome.css:32`

```css
/* load Inter font */
@import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap');
```

## Impact
- **Performance**: Without explicit font-display strategy, text may be invisible during font load (FOIT - Flash of Invisible Text) or cause layout shift (FOUT - Flash of Unstyled Text)
- **First Meaningful Paint**: Font loading blocks text rendering by default
- **Core Web Vitals**: Can negatively affect Largest Contentful Paint (LCP) and Cumulative Layout Shift (CLS)

## Evidence
The current import uses `display=swap` which is a good default, but:
1. No `<link rel="preload">` hint in HTML for critical fonts
2. No `font-display` descriptor in CSS `@font-face` for local font control
3. All 4 weights (400, 500, 600, 700) are loaded upfront even if not all are used

## Recommended Fix

**Option 1 - Use `display=optional` for better LCP:**
```css
@import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=optional');
```

**Option 2 - Add preload hint (requires HTML modification):**
```html
<link rel="preload" href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700" as="style">
```

**Option 3 - Subset fonts by weight (if not all weights are used):**
```css
/* Only load weights that are actually used */
@import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;600&display=swap');
```

Check which font weights are actually used:
- `font-weight: 400` (normal) - used
- `font-weight: 500` (medium) - used for links
- `font-weight: 600` (semibold) - used for headings
- `font-weight: 700` (bold) - check if actually needed

## References
- [Google Fonts - font-display](https://developer.mozilla.org/en-US/docs/Web/CSS/@font-face/font-display)
- [CSS Tricks - Complete Guide to font-display](https://css-tricks.com/complete-guide-font-display/)
- [Web.dev - Optimize font loading](https://web.dev/optimize-web-fonts/)
- [MDN - font-display](https://developer.mozilla.org/en-US/docs/Web/CSS/@font-face/font-display)
