---
title: "[MEDIUM] Semantic HTML improvements: use `<main>` and `<nav>` elements"
severity: MEDIUM
domain: cdc-badge-os
lens: session-lighthouse
labels:
  - "audit:toolgate/session-lighthouse"
---

## Summary
The page uses generic `<div>` elements for semantic regions. Adding proper semantic HTML elements improves accessibility and SEO.

**File:** `web-flasher/index.html` (lines 218-289)

## Impact
- **Accessibility**: Screen readers benefit from landmark regions
- **SEO**: Semantic structure helps search engines understand content hierarchy
- **Lighthouse score**: May flag semantic HTML best practices

## Evidence
Current structure (lines 218-289):
```html
<body>
  <div class="container">
    <header>
      <h1>CDC Badge OS</h1>
      <p class="subtitle">Web Firmware Flasher</p>
      <span class="version-badge loading" id="version-badge">Checking...</span>
    </header>

    <div class="card" style="display: flex; gap: 1rem; align-items: center;">
      <!-- Device info -->
    </div>

    <div class="card">
      <h2>Enter Bootloader Mode</h2>
      <ol class="steps">...</ol>
    </div>

    <div class="card flash-section">
      <!-- Flash button -->
    </div>

    <div class="card requirements">
      <h2>Requirements</h2>
      <ul>...</ul>
    </div>

    <footer>
      <a href="...">CDC Badge OS on GitHub</a>
    </footer>
  </div>
</body>
```

Missing:
- `<main>` element for primary content
- `<nav>` for navigation (if applicable)

## Recommended Fix
Add semantic elements:

```html
<body>
  <div class="container">
    <header>
      <h1>CDC Badge OS</h1>
      <p class="subtitle">Web Firmware Flasher</p>
      <span class="version-badge loading" id="version-badge">Checking...</span>
    </header>

    <main>
      <article>
        <div class="card">
          <!-- Device info -->
        </div>
      </article>

      <section aria-labelledby="bootloader-heading">
        <div class="card">
          <h2 id="bootloader-heading">Enter Bootloader Mode</h2>
          <ol class="steps">...</ol>
        </div>
      </section>

      <section aria-labelledby="flash-heading">
        <div class="card flash-section">
          <h2 id="flash-heading">Flash Firmware</h2>
          <!-- Flash button -->
        </div>
      </section>

      <section aria-labelledby="requirements-heading">
        <div class="card requirements">
          <h2 id="requirements-heading">Requirements</h2>
          <ul>...</ul>
        </div>
      </section>
    </main>

    <footer>
      <a href="...">CDC Badge OS on GitHub</a>
    </footer>
  </div>
</body>
```

## References
- [MDN `<main>` element](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/main)
- [MDN `<section>` element](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/section)
- [W3C ARIA in HTML](https://www.w3.org/TR/html-aria/)
