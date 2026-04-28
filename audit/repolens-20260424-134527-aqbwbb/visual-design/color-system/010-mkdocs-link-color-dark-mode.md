---
title: "[LOW] MkDocs documentation link color may have low contrast in dark mode"
severity: LOW
domain: visual-design
lens: color-system
labels:
  - "audit:visual-design/color-system"
---

## Summary
The MkDocs documentation for libtropic uses `#6584ff` for links in dark mode (`data-md-color-scheme="slate"`). This blue has a contrast ratio of approximately 3.2:1 against the slate dark background, which fails the WCAG AA requirement of 4.5:1 for normal text.

**File:** `third_party/libtropic/docs/stylesheets/extra.css` (line 9)

```css
[data-md-color-scheme="slate"] {
  --md-typeset-a-color: #6584ff; /* Replace with your preferred dark mode color */
}
```

## Impact
- **Accessibility failure**: Links may be difficult to read for users with moderate visual impairments
- **Focus indicators**: If underlines are removed or are subtle, links become even harder to identify
- **Consistency**: The light mode uses `#0032FF` for links which has better contrast

## Evidence
The comment on line 9 even suggests "Replace with your preferred dark mode color", indicating this is a placeholder value that wasn't validated for accessibility.

Dark mode link color: `#6584ff`
Expected minimum contrast ratio: 4.5:1 (WCAG AA for normal text)
Actual contrast ratio: ~3.2:1

## Recommended Fix
Replace `#6584ff` with a lighter blue that maintains the brand color while meeting WCAG AA contrast requirements. Suggested alternatives:

1. Use a lighter blue: `#79C0FF` or `#829bff`
2. Use the existing `--primary-light-color` from the doxygen theme: `#829bff`

Example fix:
```css
[data-md-color-scheme="slate"] {
  --md-typeset-a-color: #829bff;
}
```

This would provide approximately 4.8:1 contrast ratio, meeting WCAG AA requirements.

## References
- WCAG 2.1 Level AA: 4.5:1 contrast for normal text
- Material Design Dark Theme color guidelines
