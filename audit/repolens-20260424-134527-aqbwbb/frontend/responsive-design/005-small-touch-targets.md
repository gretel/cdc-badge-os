---
title: "[LOW] Touch target size for inline controls may be too small"
severity: LOW
domain: frontend
lens: responsive-design
labels:
  - responsive-design
  - touch-interactions
  - accessibility
---

## Summary
The TinyUSB WebUSB Serial website uses inline controls with small tap targets. The select dropdown and checkbox controls in the `.controls-section` may be below the recommended 44x44px minimum touch target size.

**Location**: `managed_components/espressif__tinyusb/examples/device/webusb_serial/website/index.html:28-41`

```html
<label for="newline_mode_select" class="controls">
  Command newline mode:
  <select id="newline_mode_select">
    <option value="CR">Only \r</option>
    <option value="CRLF">\r\n</option>
    <option value="ANY" selected>\r, \n or \r\n</option>
  </select>
</label>
<label for="auto_reconnect_checkbox" class="controls">
  <input type="checkbox" id="auto_reconnect_checkbox" />
  Auto Reconnect WebUSB
</label>
```

The CSS defines padding of `0.5rem 1rem` for buttons but inline form controls (select, checkbox) have minimal touch targets.

## Impact
**Minor usability impact** on touch devices:
- Small checkbox (default browser styling, typically ~16px square)
- Select dropdown may be hard to tap accurately on mobile
- Inline labels reduce effective touch target area
- Users may accidentally tap adjacent controls

## Evidence
**Location**: `managed_components/espressif__tinyusb/examples/device/webusb_serial/website/style.css`

Button styling (good - lines 161-165):
```css
.btn {
  padding: 0.5rem 1rem;
  font-size: 1rem;
  border: none;
  border-radius: 0.3rem;
  cursor: pointer;
}
```

But inline controls lack specific touch-friendly styling:
- No minimum height/width for `.controls` elements
- No `:focus` or `:active` states for keyboard/touch feedback
- Checkbox and select use default browser sizing

The controls section layout (lines 60-65):
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

## Recommended Fix
Add touch-friendly styling for inline controls:

```css
/* Minimum touch target size for inline controls */
.controls select,
.controls input[type="checkbox"] {
  min-height: 44px;
  min-width: 44px;
  margin: 2px; /* Add spacing around small targets */
}

/* Style checkbox for better visibility */
.controls input[type="checkbox"] {
  width: 20px;
  height: 20px;
  cursor: pointer;
  accent-color: #0078d7; /* Match theme color */
}

/* Style select for better touch */
.controls select {
  padding: 0.5rem;
  font-size: 1rem;
  min-height: 44px;
  border: 2px solid #ddd;
  border-radius: 4px;
  cursor: pointer;
}

/* Focus states for keyboard/touch feedback */
.controls select:focus,
.controls input[type="checkbox"]:focus {
  outline: 2px solid #0078d7;
  outline-offset: 2px;
}

/* Active state for touch feedback */
.controls select:active,
.controls input[type="checkbox"]:active {
  opacity: 0.8;
}
```

Also consider wrapping long labels on mobile:
```css
@media screen and (max-width: 767px) {
  .controls-section {
    flex-direction: column;
    align-items: flex-start;
  }
  
  .controls {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    width: 100%;
  }
}
```

## References
- [WCAG 2.1 - Target Size (2.5.8)](https://www.w3.org/WAI/WCAG21/Understanding/target-size-minimum.html)
- [MDN - Touch events](https://developer.mozilla.org/en-US/docs/Web/API/Touch_events)
- [Material Design - Touch targets](https://m2.material.io/design/usability/accessibility.html#touch-targets)
