---
title: "[MEDIUM] TinyUSB WebUSB example has RTL layout issues"
severity: MEDIUM
domain: managed_components/espressif__tinyusb
lens: rtl-layout
labels:
  - "audit:adaptive-ux/rtl-layout"
---

## Summary

The TinyUSB WebUSB Serial example website (`managed_components/espressif__tinyusb/examples/device/webusb_serial/website/`) has RTL layout issues:

1. **style.css line 104**: `text-align: left;` in `.command-history-entry`
2. **style.css lines 128-132**: Hardcoded `left: 0; right: 0;` in `.scrollbox` positioning
3. Missing `dir` attribute in `index.html`

## Impact

When displayed in RTL contexts:
- Command history entries will align to the wrong side
- Scrollbox positioning uses physical properties that don't flip
- No RTL text direction handling for any content

## Evidence

**File**: `managed_components/espressif__tinyusb/examples/device/webusb_serial/website/style.css`

**Line 104** - Text alignment:
```css
.command-history-entry {
  ...
  text-align: left;  /* Should be text-align: start */
  cursor: pointer;
}
```

**Lines 128-132** - Hardcoded positioning:
```css
.scrollbox {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;  /* Should use logical properties */
  bottom: 0;
  ...
}
```

**File**: `managed_components/espressif__tinyusb/examples/device/webusb_serial/website/index.html`

**Line 2** - No dir attribute:
```html
<html lang="en">
```

## Recommended Fix

1. Replace `text-align: left;` with `text-align: start;`

2. Replace physical positioning with logical:
```css
.scrollbox {
  position: absolute;
  top: 0;
  inset-inline-start: 0;
  inset-inline-end: 0;
  bottom: 0;
  ...
}
```

3. Add `dir="ltr"` to the HTML element for explicit direction, or add JavaScript to detect and set direction based on locale.

Note: Since this is a third-party example, these changes would need to be either:
- Applied as local overrides in a custom wrapper
- Contributed upstream to the TinyUSB project

## References

- [CSS Logical Properties - MDN](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_logical_properties_and_values)
- [W3C i18n - CSS for RTL](https://www.w3.org/International/questions/qa-css-rtl)
