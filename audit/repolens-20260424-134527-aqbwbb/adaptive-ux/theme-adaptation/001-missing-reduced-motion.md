---
title: "[HIGH] Missing `prefers-reduced-motion` support for CSS transitions and animations"
severity: HIGH
domain: adaptive-ux/theme-adaptation
lens: theme-adaptation
labels:
  - accessibility
  - motion
---

## Summary
The codebase contains multiple CSS transitions and animations but lacks any `prefers-reduced-motion` media query overrides to respect users who need motion reduced. Affected files:

1. **`web-flasher/index.html`** (line 168): `transition: background 0.15s;` on the flash button
2. **`doxygen_output/html/doxygen.css`** (multiple lines):
   - Line 454-458: `transition: text-shadow 0.5s linear;` (5 vendor prefixes)
   - Line 817-826: `transition-property: background-color, box-shadow; transition-duration: 0.5s;`
   - Line 1102-1103: `transition-property: background-color, box-shadow; transition-duration: 0.5s;`
   - Line 1521: `transition: opacity 0.3s ease;`
3. **`third_party/libtropic/docs/doxygen/html/doxygen-awesome.css`** (lines 488-499): `@keyframes slideInMenu` animation
4. **`third_party/libtropic/docs/doxygen/html/doxygen-awesome.css`** (lines 670-679): `@keyframes slideInSearchResults` animation

## Impact
Users with vestibular disorders, motion sensitivity, or who prefer reduced motion (enabled in OS settings) will experience:
- Unnecessary visual motion that may cause dizziness or nausea
- No opt-out path for decorative animations
- Potential accessibility compliance gaps (WCAG 2.1 Level AA requires providing option to reduce motion)

## Evidence
```css
/* web-flasher/index.html:168 */
transition: background 0.15s;

/* doxygen_output/html/doxygen.css:454-458 */
-webkit-transition: text-shadow 0.5s linear;
-moz-transition: text-shadow 0.5s linear;
-ms-transition: text-shadow 0.5s linear;
-o-transition: text-shadow 0.5s linear;
transition: text-shadow 0.5s linear;

/* doxygen_output/html/doxygen.css:1521 */
transition: opacity 0.3s ease;
```

No `@media (prefers-reduced-motion: reduce)` queries exist in any of these files.

## Recommended Fix
Add `prefers-reduced-motion` overrides to each stylesheet:

```css
@media (prefers-reduced-motion: reduce) {
  /* web-flasher/index.html */
  esp-web-install-button button {
    transition: none;
  }

  /* doxygen.css and doxygen-awesome.css */
  *,
  *::before,
  *::after {
    animation-duration: 0.01ms !important;
    animation-iteration-count: 1 !important;
    transition-duration: 0.01ms !important;
  }

  /* Disable keyframe animations */
  @keyframes slideInMenu {
    to { transform: translate(0px, 0px); }
  }
  @keyframes slideInSearchResults {
    to { transform: translate(0, 20px); }
  }
}
```

Alternatively, use `animation: none;` and `transition: none;` within the media query for specific elements.

## References
- [WCAG 2.1 Criterion 2.3.3: Animation from Interactions](https://www.w3.org/TR/WCAG21/#animation-from-interactions)
- [MDN: prefers-reduced-motion](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/prefers-reduced-motion)
- [CSS-Tricks: A Complete Guide to prefers-reduced-motion](https://css-tricks.com/a-complete-guide-to-prefers-reduced-motion/)
