---
title: "[HIGH] Two-column layout lacks mobile breakpoint for narrow screens"
severity: HIGH
domain: managed_components/espressif__tinyusb
lens: adaptive-content
labels:
  - "responsive-layout"
  - "mobile-usability"
  - "flexbox"
---

## Summary
The TinyUSB WebUSB Serial example (`/input/20260423-132359-oj8ayc/cdc-badge-os/managed_components/espressif__tinyusb/examples/device/webusb_serial/website/index.html` and `style.css`) has **zero responsive breakpoints**. The two-column layout (Command History + Received Data) uses `display: flex` with `.column` children that have `flex: 1`, forcing them to share horizontal space equally regardless of viewport width.

**Evidence:**
```css
/* style.css lines 70-80 */
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
```

On a typical mobile screen (360-425px width):
- Each column gets ~170px (minus padding and resizer)
- The resizer (5px) adds minimal value on mobile
- Text in `.scrollbox` with `white-space: nowrap` will overflow horizontally
- The input field at the bottom becomes cramped

No media queries exist in the entire `style.css` file to adapt the layout for smaller screens.

## Impact
- **Mobile usability broken**: Users on phones see an unusable two-column layout
- **Horizontal scroll**: Content overflows due to `white-space: nowrap` on narrow columns
- **Touch targets**: Buttons and controls are too small for touch interaction
- **No alternative**: No stacking, no tabs, no "show one at a time" pattern offered

## Evidence
- File: `managed_components/espressif__tinyusb/examples/device/webusb_serial/website/style.css`
- Lines: 70-80 (io-container and column definitions)
- Line: 135 (`white-space: nowrap` compounds the problem)
- Total media queries in file: **0**

HTML structure (lines 50-75 of `index.html`):
```html
<div class="io-container">
  <section class="column">
    <h2>Command History</h2>
    <div id="command_history_scrollbox" class="scrollbox monospaced"></div>
    <input id="command_line_input" class="input" />
  </section>
  <div class="resizer" id="resizer"></div>
  <section class="column">
    <h2>Received Data</h2>
    <div id="received_data_scrollbox" class="scrollbox monospaced"></div>
  </section>
</div>
```

## Recommended Fix
Add a mobile breakpoint that stacks the columns vertically:

```css
/* Mobile-first: stack columns on narrow screens */
@media (max-width: 768px) {
  .io-container {
    flex-direction: column;
  }
  
  .resizer {
    display: none; /* Hide resizer on mobile */
  }
  
  .column {
    flex: 1;
    min-height: 0; /* Allow flex items to shrink */
  }
  
  /* Make scrollboxes take available height */
  .scrollbox-wrapper {
    flex: 1;
    min-height: 150px; /* Minimum usable height */
  }
  
  /* Allow text wrapping on mobile */
  .scrollbox {
    white-space: normal;
  }
  
  /* Stack controls vertically */
  .controls-section {
    flex-direction: column;
  }
  
  .controls.btn,
  .controls label {
    width: 100%;
  }
}
```

Alternative approach for very small screens:
- Show only one column at a time with tab buttons
- Add a toggle to switch between "Command History" and "Received Data" views
- Hide the resizer completely on mobile

## References
- [MDN: CSS Media Queries](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_media_queries/Using_media_queries)
- [CSS-Tricks: Complete Guide to Flexbox](https://css-tricks.com/snippets/css/a-guide-to-flexbox/)
- [Web.dev: Responsive design basics](https://web.dev/responsive-web-design-basics/)
