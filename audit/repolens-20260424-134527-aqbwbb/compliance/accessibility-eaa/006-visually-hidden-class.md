---
title: "[LOW] Missing visually-hidden class definition"
severity: LOW
domain: web-flasher
lens: accessibility-eaa
labels:
  - "wcag-2.1"
  - "css"
  - "screen-readers"
---

## Summary
The web flasher CSS at `web-flasher/index.html` (lines 11-207) does not include a `.visually-hidden` (or `.sr-only`) class for providing text that is available to screen readers but hidden visually.

## Impact
When developers need to add accessible labels (e.g., for icons, buttons, or dynamic content), they have no established pattern for visually hiding text. This can lead to inconsistent implementation or skip links that are not properly hidden.

## Evidence
File: `web-flasher/index.html` (lines 11-207) - CSS styles defined but no visually-hidden utility class.

## Recommended Fix
Add a visually-hidden utility class to the CSS:

```css
.visually-hidden {
  position: absolute;
  width: 1px;
  height: 1px;
  padding: 0;
  margin: -1px;
  overflow: hidden;
  clip: rect(0, 0, 0, 0);
  white-space: nowrap;
  border: 0;
}

/* Show on focus for skip links */
.visually-hidden:focus {
  position: static;
  width: auto;
  height: auto;
  padding: inherit;
  margin: inherit;
  clip: auto;
  white-space: normal;
}
```

## References
- [W3C CSS Techniques for Hiding Content](https://www.w3.org/WAI/WCAG21/Techniques/css/C7)
- [Modern Screen Reader Only Class](https://www.a11yproject.com/posts/how-to-hide-content/)
