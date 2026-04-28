---
title: "[MEDIUM] Missing skip navigation link for keyboard users"
severity: MEDIUM
domain: cdc-badge-os
lens: session-lighthouse
labels:
  - "audit:toolgate/session-lighthouse"
---

## Summary
The page lacks a "Skip to main content" link, which is helpful for keyboard users to bypass repetitive content.

**File:** `web-flasher/index.html` (entire page structure)

## Impact
- **Accessibility**: Keyboard users must tab through all content
- **Lighthouse score**: May flag "skip-link" best practice
- **WCAG**: Recommended but not required for basic compliance

## Evidence
The page structure:
```html
<body>
  <div class="container">
    <header>...</header>
    <div class="card">...</div>  <!-- Device info -->
    <div class="card">...</div>  <!-- Instructions -->
    <div class="card">...</div>  <!-- Flash button -->
    <div class="card">...</div>  <!-- Requirements -->
    <footer>...</footer>
  </div>
</body>
```

No skip link is present at the top of the page.

## Recommended Fix
Add a skip link at the beginning of `<body>`:

```html
<body>
  <a href="#main-content" class="skip-link">Skip to main content</a>
  <div class="container">
    <header>...</header>
    <main id="main-content">
      <div class="card">...</div>
      <!-- rest of content -->
    </main>
    <footer>...</footer>
  </div>
  <style>
    .skip-link {
      position: absolute;
      top: -40px;
      left: 0;
      padding: 8px;
      background: var(--accent);
      color: var(--bg);
      z-index: 100;
      transition: top 0.2s;
    }
    .skip-link:focus {
      top: 0;
    }
  </style>
```

**Note:** Wrap main content in `<main>` element for semantic structure.

## References
- [W3C Skip Links](https://www.w3.org/WAI/tutorials/skiplinks/)
- [MDN Skip to main content](https://developer.mozilla.org/en-US/docs/Web/Accessibility/ARIA/Roles/main_role)
