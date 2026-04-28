---
title: "[MEDIUM] TinyUSB WebUSB interface has no mobile breakpoints"
severity: MEDIUM
domain: frontend
lens: responsive-design
labels:
  - responsive-design
  - mobile-first
  - css
---

## Summary
The TinyUSB WebUSB Serial website (`managed_components/espressif__tinyusb/examples/device/webusb_serial/website/style.css`) has **zero media queries** for responsive design. The entire layout uses fixed flexbox and absolute positioning that doesn't adapt to smaller screens.

**Location**: `managed_components/espressif__tinyusb/examples/device/webusb_serial/website/style.css` (296 lines, no media queries)

The CSS defines:
- Fixed `height: 100vh` on body (line 20)
- Fixed `padding: 1rem` on controls and columns (lines 61, 76)
- Fixed `width: 5px` resizer (line 211)
- No responsive adjustments for any screen size

## Impact
**Moderate to high usability impact** on mobile and tablet devices:
- Two-column layout with resizer doesn't work well on narrow screens
- Controls section with horizontal flex layout will overflow on small screens
- Fixed height scrollboxes may not fit on mobile viewports with browser chrome
- No adaptation for portrait vs. landscape orientations
- Likely horizontal scrollbars on phones (< 480px width)

## Evidence
**Current CSS structure** (no media queries at all):

1. Fixed viewport height (lines 8-20):
```css
html,
body {
  height: 100%;
  ...
}

body {
  display: flex;
  flex-direction: column;
  height: 100vh;
}
```

2. Fixed padding on controls (lines 59-67):
```css
.controls-section,
.status-section {
  padding: 1rem;
  flex-shrink: 0;
  display: flex;
  flex-direction: row;
  flex-wrap: wrap;
  gap: 0.5rem;
}
```

3. Two-column layout with resizer (lines 70-85):
```css
.io-container {
  display: flex;
  flex: 1;
  width: 100%;
  overflow: hidden;
}

.column {
  flex: 1;
  padding: 1rem;
  display: flex;
  flex-direction: column;
}

.resizer {
  width: 5px;
  background-color: #ccc;
  cursor: col-resize;
  height: 100%;
}
```

**Comparison**: The doxygen-awesome.css has 20+ media queries for responsive design, while this TinyUSB web interface has none.

## Recommended Fix
Add mobile-first responsive media queries:

```css
/* Mobile breakpoint - stack layout */
@media screen and (max-width: 767px) {
  .io-container {
    flex-direction: column;
    overflow: visible;
  }
  
  .resizer {
    width: 100%;
    height: 5px;
    cursor: row-resize;
  }
  
  .column {
    flex: none;
    height: 50vh;
  }
  
  .controls-section {
    flex-direction: column;
    align-items: stretch;
  }
  
  .controls {
    width: 100%;
  }
  
  .heading-with-controls {
    flex-direction: column;
    align-items: flex-start;
    gap: 0.5rem;
  }
  
  .btn {
    width: 100%;
  }
}

/* Small mobile - adjust font sizes */
@media screen and (max-width: 480px) {
  h1.app-title {
    font-size: 1.2rem;
  }
  
  h2 {
    font-size: 1rem;
  }
  
  .input {
    font-size: 16px; /* Prevent iOS zoom */
  }
  
  .header {
    flex-direction: column;
    gap: 0.5rem;
  }
}
```

## References
- [MDN - CSS Media Queries](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_media_queries/Using_media_queries)
- [Web.dev - Responsive Web Design Basics](https://web.dev/responsive-web-design-basics/)
- [CSS Tricks - Complete Guide to Flexbox](https://css-tricks.com/snippets/css/a-guide-to-flexbox/)
- Common breakpoints: Mobile (<768px), Tablet (768-1024px), Desktop (>1024px)
