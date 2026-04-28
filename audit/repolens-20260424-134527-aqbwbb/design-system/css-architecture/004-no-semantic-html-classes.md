---
title: "[LOW] Limited Use of Semantic HTML Elements"
severity: LOW
domain: design-system
lens: css-architecture
labels:
  - "audit:design-system/css-architecture"
---

## Summary

The web flasher uses generic `<div>` elements with classes instead of semantic HTML5 elements where appropriate, requiring more CSS selectors and potentially affecting accessibility.

## Impact

**Low impact** - The page is simple and functional. Benefits of semantic HTML:
- Better accessibility (screen readers can navigate more easily)
- More meaningful CSS selectors
- Better SEO (though less relevant for a flasher tool)
- Reduced need for wrapper `<div>` elements

## Evidence

**File:** `web-flasher/index.html`

**Current usage (lines 218-290):**
```html
<!-- Line 218: Uses div instead of main -->
<div class="container">

<!-- Line 220: Uses div instead of header -->
<header>  <!-- Good: header element used -->
  <h1>CDC Badge OS</h1>
  <p class="subtitle">Web Firmware Flasher</p>
  <span class="version-badge loading" id="version-badge">...</span>
</header>

<!-- Line 225: Uses div instead of article/section -->
<div class="card" style="display: flex; gap: 1rem; align-items: center;">
  <img src="badge.jpg" alt="CDC Badge v1.0" style="width: 80px; ...">
  <div>  <!-- Nested div for text content -->
    <p>...</p>
  </div>
</div>

<!-- Line 234: Uses div instead of section -->
<div class="card">
  <h2>Enter Bootloader Mode</h2>
  <ol class="steps">
    <!-- ... -->
  </ol>
</div>

<!-- Line 261: Uses div instead of section -->
<div class="card flash-section">
  <!-- ... -->
</div>

<!-- Line 279: Uses div instead of aside -->
<div class="card requirements">
  <!-- ... -->
</div>

<!-- Line 287: Uses div instead of footer -->
<footer>  <!-- Good: footer element used -->
```

**Inline styles (lines 225-226, 227, 230):**
```html
<div class="card" style="display: flex; gap: 1rem; align-items: center;">
  <img src="badge.jpg" alt="CDC Badge v1.0" style="width: 80px; height: auto; ...">
```

## Recommended Fix

**1. Use semantic HTML elements:**
```html
<main class="container">
  <header>...</header>
  
  <article class="card">  <!-- Device info card -->
    <!-- ... -->
  </article>
  
  <section class="card">  <!-- Bootloader instructions -->
    <!-- ... -->
  </section>
  
  <section class="card flash-section">
    <!-- ... -->
  </section>
  
  <aside class="card requirements">  <!-- Requirements sidebar -->
    <!-- ... -->
  </aside>
</main>
```

**2. Move inline styles to CSS:**
```css
.device-card {
  display: flex;
  gap: 1rem;
  align-items: center;
}

.device-card img {
  width: 80px;
  height: auto;
  border-radius: 0.5rem;
  flex-shrink: 0;
}
```

**Estimated effort:** 30-45 minutes

## References

- HTML5 Semantic Elements: https://developer.mozilla.org/en-US/docs/Web/HTML/Element
- Accessibility Best Practices: https://www.w3.org/WAI/tutorials/page-structure/
- Inline Styles Anti-pattern: https://css-tricks.com/when-inline-styles-are-the-right-choice/
