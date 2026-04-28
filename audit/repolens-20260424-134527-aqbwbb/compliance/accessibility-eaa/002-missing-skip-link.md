---
title: "[MEDIUM] Missing skip-to-main-content link"
severity: MEDIUM
domain: web-flasher
lens: accessibility-eaa
labels:
  - "wcag-2.1"
  - "eaa"
  - "bfsg"
  - "keyboard-navigation"
---

## Summary
The web flasher at `web-flasher/index.html` lacks a "Skip to main content" link, which is a key requirement for keyboard users to bypass repetitive navigation.

## Impact
Without a skip link, keyboard users must tab through all content (header, instructions, footer links) to reach the main interactive element (the flash button). This creates a poor user experience, especially for screen reader users and those with motor impairments.

Per WCAG 2.1 Success Criterion 2.4.1 (Bypass Blocks), a mechanism should be available to bypass blocks of content that are repeated on multiple pages.

## Evidence
- File: `web-flasher/index.html` (lines 1-309)
- No skip link present in the HTML structure
- The page has multiple sections (header, instructions, flash button, requirements, footer) that would require tabbing through
- Line 218: `<body>` starts directly with `.container` without any skip link

## Recommended Fix
Add a skip link as the first focusable element after the opening `<body>` tag:

```html
<body>
  <a href="#main-content" class="skip-link" style="
    position: absolute;
    top: -40px;
    left: 0;
    background: var(--accent);
    color: var(--bg);
    padding: 0.5rem 1rem;
    z-index: 100;
    transition: top 0.3s;
  ">Skip to main content</a>
  <a href="#main-content" style="display: none;">Skip to main content</a>
  
  <div class="container" id="main-content">
    <!-- rest of content -->
```

Add CSS to show the link on focus:
```css
.skip-link:focus {
  top: 0;
}
```

## References
- [WCAG 2.1 SC 2.4.1 Bypass Blocks](https://www.w3.org/WAI/WCAG21/Understanding/bypass-blocks.html)
- [W3C Skip Links Guide](https://www.w3.org/WAI/tutorials/skip-links/)
