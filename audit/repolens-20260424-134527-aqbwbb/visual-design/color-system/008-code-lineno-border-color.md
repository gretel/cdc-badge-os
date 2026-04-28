---
title: "[LOW] Code block line number border uses harsh bright green color"
severity: LOW
domain: visual-design
lens: color-system
labels:
  - "audit:visual-design/color-system"
---

## Summary
The doxygen documentation code blocks use a bright green (`#00FF00`) for the line number border color in light mode. This highly saturated color can be visually distracting and may cause eye strain during extended reading sessions.

**File:** `doxygen_output/html/doxygen.css` (line 138)

```css
--fragment-lineno-border-color: #00FF00;
```

In dark mode, this is changed to a more subdued `#30363D`, but the light mode value remains harsh.

## Impact
- **Visual comfort**: The bright green border creates unnecessary visual noise in code blocks
- **Accessibility**: May be distracting for users with visual sensitivities or attention disorders
- **Design coherence**: The bright green clashes with the otherwise muted, professional color palette of the documentation

## Evidence
The CSS variable is defined at line 138 in the light mode section:

```css
--fragment-lineno-border-color: #00FF00;
```

And used at line 897:
```css
border-right: 2px solid var(--fragment-lineno-border-color);
```

Dark mode uses a more appropriate `#30363D` (line 335).

## Recommended Fix
Replace the bright green `#00FF00` with a more subdued color that matches the neutral gray palette used elsewhere in the documentation. Suggested alternatives:

1. Use the same color as dark mode: `#30363D`
2. Use a lighter neutral gray: `#C4CFE5` (matches `--nav-border-color`)
3. Use a subtle blue-gray: `#D5DDEC` (matches `--memdecl-border-color`)

Example fix:
```css
--fragment-lineno-border-color: #C4CFE5;
```

## References
- WCAG 2.1: Visual comfort and reducing visual clutter
- Design systems typically use neutral grays for structural/border elements in code blocks
