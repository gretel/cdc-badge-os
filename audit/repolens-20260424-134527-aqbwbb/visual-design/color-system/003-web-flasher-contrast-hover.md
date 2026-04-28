---
title: "[LOW] Link hover state may lack sufficient contrast change for accessibility"
severity: LOW
domain: visual-design
lens: color-system
labels:
  - "audit:visual-design/color-system"
---

## Summary
The web-flasher uses `var(--accent)` for links and `var(--accent-hover)` for hover states. While both colors have good contrast against the background, the hover state may not provide enough visual differentiation for users who rely on color changes (rather than underlines) to identify interactive elements.

**File:** `web-flasher/index.html` (lines 18-19, 208-210)

## Impact
- **Accessibility:** Users with color vision deficiencies may not notice the hover state change
- **User experience:** The subtle color shift (`#58a6ff` to `#79c0ff`) may be missed on some displays or in bright lighting conditions
- **WCAG compliance:** While the contrast ratios are sufficient, the change in luminance between states is relatively small

## Evidence
**Color definitions (lines 18-19):**
```css
--accent: #58a6ff;      /* Light blue, luminance ~0.45 */
--accent-hover: #79c0ff; /* Lighter blue, luminance ~0.60 */
```

**Link styling (lines 208-210):**
```css
footer a {
  color: var(--accent);
  text-decoration: none;
}

footer a:hover {
  text-decoration: underline;  /* Only visual change is underline */
}
```

The luminance difference between `#58a6ff` and `#79c0ff` is approximately 0.15, which is noticeable but could be more pronounced. Additionally, the only visual change on hover is the addition of an underline; the color becomes lighter which may be subtle on some displays.

## Recommended Fix
Enhance the hover state for better visual differentiation:

**Option 1 - Increase color contrast:**
```css
--accent: #58a6ff;
--accent-hover: #a5d6ff;  /* Lighter, more noticeable shift */
```

**Option 2 - Add additional visual indicators:**
```css
footer a:hover {
  text-decoration: underline;
  text-underline-offset: 3px;  /* More prominent underline */
}
```

**Option 3 - Add focus indicator (for keyboard users):**
```css
footer a:focus {
  outline: 2px solid var(--accent);
  outline-offset: 2px;
}
```

Any of these options (or a combination) would improve accessibility without affecting the overall design.

## References
- [WCAG 2.1 - Contrast (Minimum)](https://www.w3.org/WAI/WCAG21/Understanding/contrast-minimum.html)
- [WCAG 2.1 - Focus Visible](https://www.w3.org/WAI/WCAG21/Understanding/focus-visible.html)
- [MDN - :hover pseudo-class](https://developer.mozilla.org/en-US/docs/Web/CSS/:hover)
