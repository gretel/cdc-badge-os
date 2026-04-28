---
title: "[LOW] Code comment color has borderline contrast in light mode"
severity: LOW
domain: visual-design
lens: color-system
labels:
  - "audit:visual-design/color-system"
---

## Summary
The doxygen documentation code block comments use dark red (`#800000`) for light mode. While this color is distinct, it has a contrast ratio of approximately 7.5:1 against the white background, which passes WCAG AA but may be harder to read than neutral gray comments.

**File:** `doxygen_output/html/doxygen.css` (line 126)

```css
--code-comment-color: #800000;
```

## Impact
- **Readability**: Dark red comments can be harder to scan quickly compared to neutral gray
- **Visual hierarchy**: The strong hue may draw too much attention to comments vs. actual code
- **Consistency**: Most modern IDEs and documentation use gray for comments, making this choice non-standard

## Evidence
The CSS variable at line 126:
```css
--code-comment-color: #800000;
```

Used at line 1019:
```css
color: var(--code-comment-color);
```

In dark mode (line 323), comments use a more standard gray: `#717790`.

## Recommended Fix
Consider changing the comment color to a neutral gray that maintains good contrast while being less visually dominant. Suggested alternatives:

1. Use a medium gray: `#666666` or `#717790` (matching dark mode)
2. Use a blue-gray: `#6A737D`

Example fix:
```css
--code-comment-color: #6A737D;
```

This would improve readability and align with common code highlighting conventions.

## References
- Common syntax highlighting schemes (GitHub, VS Code, Atom) use gray for comments
- WCAG 2.1: Contrast requirements for text readability
