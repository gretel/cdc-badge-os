---
title: "[LOW] Missing skip link and landmark roles for navigation"
severity: LOW
domain: frontend
lens: a11y
labels:
  - "audit:frontend/accessibility"
---

## Summary
The web flasher page lacks skip navigation links and proper landmark roles. While the page is relatively simple, adding these elements improves navigation efficiency for keyboard and screen reader users.

**Location:** `web-flasher/index.html`, entire document structure

### Current Structure (lines 218-289):
```html
<body>
  <div class="container">
    <header>
      <h1>CDC Badge OS</h1>
      <p class="subtitle">Web Firmware Flasher</p>
      <span class="version-badge loading" id="version-badge">Checking latest version...</span>
    </header>
    <!-- Multiple card sections -->
    <footer>
      <a href="https://github.com/krim404/cdc-badge-os">CDC Badge OS on GitHub</a>
    </footer>
  </div>
</body>
```

## Impact
- **Keyboard users must tab through all elements** - No way to skip to main content
- **Screen reader users lack navigation landmarks** - Cannot quickly jump between sections
- **WCAG 2.1 Success Criterion 2.4.1 (Bypass Blocks)** - Not fully met
- Users experience slower navigation compared to pages with proper landmarks

## Evidence
- Line 218: `<body>` tag without skip link
- Line 219: `<div class="container">` used instead of `<main>` landmark
- Line 219-288: Container div lacks semantic meaning
- No `<nav>` element for the steps list
- No `role="main"`, `role="contentinfo"`, or equivalent landmark roles
- The steps list (lines 244-249) is an `<ol>` but not wrapped in a `<nav>` or given a label

## Recommended Fix
Add skip link and landmark roles:

```html
<body>
  <a href="#main-content" class="skip-link">Skip to main content</a>
  
  <div class="container">
    <header role="banner">
      <h1>CDC Badge OS</h1>
      <p class="subtitle">Web Firmware Flasher</p>
      <span class="version-badge loading" id="version-badge">Checking latest version...</span>
    </header>

    <main id="main-content" role="main">
      <!-- All card sections here -->
      
      <nav aria-label="Flashing steps">
        <ol class="steps">
          <!-- Steps -->
        </ol>
      </nav>
    </main>

    <footer role="contentinfo">
      <a href="https://github.com/krim404/cdc-badge-os">CDC Badge OS on GitHub</a>
    </footer>
  </div>
</body>
```

Add CSS for skip link:
```css
.skip-link {
  position: absolute;
  top: -40px;
  left: 0;
  background: var(--accent);
  color: var(--bg);
  padding: 0.5rem 1rem;
  z-index: 100;
  transition: top 0.2s;
}

.skip-link:focus {
  top: 0;
}
```

## References
- [WCAG 2.1 Success Criterion 2.4.1 Bypass Blocks](https://www.w3.org/TR/WCAG21/#bypass-blocks)
- [MDN: Landmark roles](https://developer.mozilla.org/en-US/docs/Web/Accessibility/ARIA/Roles)
- [WAI-ARIA Authoring Practices - Skip Links](https://www.w3.org/WAI/WCAG21/Techniques/html/H59)
