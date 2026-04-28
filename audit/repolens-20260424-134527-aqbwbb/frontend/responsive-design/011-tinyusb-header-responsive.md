---
title: "[MEDIUM] TinyUSB web interface header doesn't stack on mobile"
severity: MEDIUM
domain: frontend
lens: responsive-design
labels:
  - responsive-design
  - mobile-first
  - css
---

## Summary
The TinyUSB WebUSB Serial header (`.header` in `managed_components/espressif__tinyusb/examples/device/webusb_serial/website/style.css:23-30`) uses a horizontal flex layout that doesn't stack on narrow screens. The title, theme button, and GitHub link all stay on one line, causing overflow on mobile.

**Location**: `managed_components/espressif__tinyusb/examples/device/webusb_serial/website/style.css:23-30`

```css
.header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0.5em 1em;
  gap: 1em;
  flex-shrink: 0;
}
```

## Impact
**Moderate usability impact** on mobile devices:
- Header elements overflow on screens narrower than ~500px
- GitHub link text may be cut off or wrap awkwardly
- Theme button may become too small on narrow screens
- No adaptation for portrait vs. landscape orientations
- Horizontal scrollbar may appear on small phones

## Evidence
**Current header HTML structure** (`index.html:15-22`):
```html
<header class="header">
  <h1 class="app-title">TinyUSB - WebUSB Serial</h1>
  <button id="theme-toggle" class="btn btn-theme">Theme: Auto</button>
  <a class="github-link" href="..." target="_blank">
    Find my source on GitHub
  </a>
</header>
```

**Current CSS** (lines 23-30):
- `display: flex` with horizontal layout
- `justify-content: space-between` - elements spread across full width
- No `flex-wrap` defined (defaults to `nowrap`)
- No media queries for responsive behavior

**Result**: On a 360px wide screen (common mobile width):
- Header needs: ~200px (title) + ~80px (button) + ~120px (link) = ~400px minimum
- Available space: 360px minus padding = ~320px
- Overflow: ~80px

## Recommended Fix
Add responsive media queries to stack header on mobile:

```css
.header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0.5em 1em;
  gap: 1em;
  flex-shrink: 0;
  flex-wrap: wrap; /* Allow wrapping */
}

/* Mobile: stack header elements */
@media screen and (max-width: 600px) {
  .header {
    flex-direction: column;
    align-items: flex-start;
    gap: 0.5rem;
  }
  
  .app-title {
    font-size: 1.2rem;
    width: 100%;
    text-align: center;
  }
  
  .header .btn,
  .header .github-link {
    width: 100%;
    text-align: center;
  }
  
  .github-link {
    font-size: 0.9rem;
  }
}

/* Very small screens */
@media screen and (max-width: 360px) {
  .app-title {
    font-size: 1rem;
  }
  
  .btn {
    padding: 0.4rem 0.8rem;
    font-size: 0.9rem;
  }
}
```

## References
- [MDN - CSS Flexbox](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_Flexible_Box_Layout)
- [CSS Tricks - A Complete Guide to Flexbox](https://css-tricks.com/snippets/css/a-guide-to-flexbox/)
- [Web.dev - Responsive Web Design Basics](https://web.dev/responsive-web-design-basics/)
- Common breakpoints: Mobile (<600px), Tablet (600-1024px), Desktop (>1024px)
