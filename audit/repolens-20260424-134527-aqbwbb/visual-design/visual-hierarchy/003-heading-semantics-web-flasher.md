---
title: "[LOW] Heading semantics in web-flasher - h1 followed by two h2s without intermediate levels"
severity: LOW
domain: visual-design
lens: visual-hierarchy
labels:
  - "audit:visual-design/visual-hierarchy"
---

## Summary
The web-flasher `index.html` uses heading tags that skip levels. The structure is:
- Line 221: `<h1>CDC Badge OS</h1>`
- Line 240: `<h2>Enter Bootloader Mode</h2>`
- Line 278: `<h2>Requirements</h2>`

While this is technically valid (two h2s under one h1), the visual hierarchy could be improved. The numbered steps inside "Enter Bootloader Mode" use `<ol class="steps">` with `<li>` elements that have visual emphasis (`<strong>`) but no heading structure. This creates a flat hierarchy where sub-sections lack semantic importance.

Additionally, the subtitle "Web Firmware Flasher" (line 222) is a `<p>` tag rather than being part of the heading structure, and the version badge is a `<span>` with no semantic relationship to the title.

## Impact
1. **Accessibility**: Screen reader users navigating by headings may find the structure confusing when important sub-sections (like the numbered steps) lack heading semantics.
2. **Visual hierarchy ambiguity**: Without heading levels, the relationship between main sections and their content is less clear.
3. **SEO**: Heading structure helps search engines understand content organization.

## Evidence
```html
<!-- web-flasher/index.html:221-240 -->
<header>
  <h1>CDC Badge OS</h1>
  <p class="subtitle">Web Firmware Flasher</p>
  <span class="version-badge loading" id="version-badge">Checking latest version...</span>
</header>

<!-- Line 240 -->
<div class="card">
  <h2>Enter Bootloader Mode</h2>
  <p style="color: var(--text-muted); font-size: 0.9rem; margin-bottom: 0.75rem;">
    On the back of the badge, locate the <strong>FLASH</strong> and <strong>RESET</strong> buttons.
  </p>
  <ol class="steps">
    <li>Press and <strong>hold FLASH</strong></li>
    <!-- No heading for steps - they are visually important but semantically flat -->
  </ol>
</div>
```

## Recommended Fix
1. **Add semantic headings for sub-sections** within the steps card:
```html
<div class="card">
  <h2>Enter Bootloader Mode</h2>
  <p>On the back of the badge, locate the <strong>FLASH</strong> and <strong>RESET</strong> buttons.</p>
  <h3>Steps</h3>
  <ol class="steps">
    <li>Press and <strong>hold FLASH</strong></li>
    ...
  </ol>
</div>
```

2. **Consider using `<hgroup>`** for the header to group title and subtitle:
```html
<header>
  <hgroup>
    <h1>CDC Badge OS</h1>
    <p class="subtitle">Web Firmware Flasher</p>
  </hgroup>
  <span class="version-badge">...</span>
</header>
```

3. **Alternative**: Use `<h2>` for card titles and `<h3>` for the steps to create a clearer hierarchy.

## References
- [MDN: Heading elements](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/Heading_Elements)
- [W3C: Using heading levels](https://www.w3.org/WAI/tutorials/page-structure/headings/)
- [Hamburger: Don't skip heading levels](https://hamburger.github.io/accessibility/heading-levels/)
