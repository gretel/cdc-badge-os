---
title: "[LOW] Web flasher page lacks section navigation for long scrolling content"
severity: LOW
domain: information-architecture
lens: content-hierarchy
labels:
  - "audit:information-architecture/content-hierarchy"
---

## Summary
The web flasher page (`web-flasher/index.html:1-309`) contains multiple content sections (header, device info, boot steps, flash button, requirements) but lacks:
- In-page navigation or table of contents
- Anchor links for deep-linking to specific sections
- Sticky section headers for long-form scrolling
- Clear visual hierarchy between primary actions (flash) and secondary info (requirements)

All content flows linearly without structural markers for quick scanning.

## Impact
Users returning to specific sections (e.g., "how do I enter bootloader mode?") must scroll through the entire page. No deep-linking support means users cannot bookmark or share links to specific sections.

## Evidence
File: `web-flasher/index.html:210-280`
```html
<div class="container">
  <header>...</header>
  <div class="card">Device info...</div>
  <div class="card">Enter Bootloader Mode</div>
  <div class="card">Flash section</div>
  <div class="card">Requirements</div>
  <footer>...</footer>
</div>
```

Each section is a `.card` div but there are no `<section>` tags, heading levels (`<h2>`, `<h3>`), or anchor IDs for navigation.

## Recommended Fix
1. Add `<section>` tags with `id` attributes to each major card
2. Add a sticky table of contents sidebar or top navigation with anchor links
3. Use proper heading hierarchy (`<h1>` for title, `<h2>` for section titles)
4. Add "Back to top" link at the bottom

Approximate effort: 1 hour to add semantic HTML structure and basic navigation.

## References
- Web flasher HTML: `web-flasher/index.html`
- Content sections: header, boot steps, flash, requirements
