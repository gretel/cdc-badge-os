---
title: "[MEDIUM] Web Flasher lacks print stylesheet for printable content"
severity: MEDIUM
domain: adaptive-ux
lens: print-stylesheet
labels:
  - audit:adaptive-ux/print-stylesheet
---

## Summary
The web-flasher application (`web-flasher/index.html`) has no `@media print` rules or print-specific stylesheet. The page contains instructional content (step-by-step bootloader instructions, requirements list, notices) that users may want to print for reference during the flashing process.

**File:** `web-flasher/index.html` (lines 1-309)
**CSS Location:** Inline styles in `<head>` (lines 11-216)

## Impact
- Users printing the page (e.g., to have instructions handy while flashing) will get browser defaults
- Dark theme colors (`--bg: #0d1117`, `--surface: #161b22`) waste ink and may print poorly
- Interactive elements like the flash button will print unnecessarily
- Background colors and borders may not translate well to print
- No page-break control for the multi-step instructions

## Evidence
The stylesheet contains only a single responsive media query:
```css
@media (max-width: 480px) {
  .container { padding: 1.25rem 1rem; }
  header h1 { font-size: 1.4rem; }
}
```

No `@media print` block exists. The page has:
- Dark background colors (lines 12-22): `--bg: #0d1117`, `--surface: #161b22`
- Interactive flash button (lines 165-175)
- Multi-step ordered list (lines 243-248)
- Notice/warning box (lines 126-141)

## Recommended Fix
Add a `@media print` block after line 215 with:

```css
@media print {
  /* Hide interactive elements */
  esp-web-install-button, .flash-section { display: none; }
  
  /* Light background for paper */
  body { background: white; color: black; }
  
  /* Simplify card styling */
  .card { border: 1px solid #ccc; break-inside: avoid; }
  
  /* Prevent mid-step breaks */
  .steps li { break-inside: avoid; }
  
  /* Adjust headings */
  h1, h2, h3 { break-after: avoid; }
  
  /* Remove dark backgrounds */
  .notice { background: #f9f9f9; border-color: #ccc; }
  
  /* Show URLs for links */
  a[href]:after { content: " (" attr(href) ")"; font-size: 0.8em; }
}
```

## References
- [MDN: @media print](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/print)
- [CSS Paged Media Module](https://www.w3.org/TR/css-page-3/)
