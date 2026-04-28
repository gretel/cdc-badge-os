---
title: "[MEDIUM] Legacy 100vh usage in WebUSB Serial example causes mobile overflow"
severity: MEDIUM
domain: adaptive-ux/viewport-sizing
lens: viewport-sizing
labels:
  - "audit:adaptive-ux/viewport-sizing"
---

## Summary
The TinyUSB WebUSB Serial example website (`managed_components/espressif__tinyusb/examples/device/webusb_serial/website/style.css`) uses `height: 100vh` on the body element (line 19), causing layout overflow on mobile browsers.

**File:** `managed_components/espressif__tinyusb/examples/device/webusb_serial/website/style.css`  
**Line:** 19  
**CSS Property:** `height: 100vh`

```css
body {
  display: flex;
  flex-direction: column;
  height: 100vh;  /* Line 19 - problematic on mobile */
}
```

## Impact
- Content extends below the fold on mobile devices with address bars
- The flex layout with `height: 100vh` combined with child elements can cause scrollbars
- Users may miss critical controls (Connect/Disconnect buttons) that render behind browser chrome
- The WebUSB Serial tool is typically used on mobile for field flashing, making this more critical

## Evidence
From `managed_components/espressif__tinyusb/examples/device/webusb_serial/website/style.css:19`:
```css
body {
  display: flex;
  flex-direction: column;
  height: 100vh;
}
```

Also note line 10-11:
```css
html,
body {
  height: 100%;
  ...
}
```

The combination of `height: 100%` on html/body and `height: 100vh` on body creates potential for double-counting viewport height.

## Recommended Fix
Replace `height: 100vh` with dynamic viewport units:

```css
body {
  display: flex;
  flex-direction: column;
  height: 100vh;
  height: 100dvh;  /* Dynamic viewport height */
  min-height: 100svh;  /* Smallest viewport height for mobile */
}
```

**Additional improvement:** Remove redundant `height: 100%` from html/body since `dvh` handles this:

```css
html,
body {
  font-family: sans-serif;
  background: #f5f5f5;
  color: #333;
  /* Remove height: 100%; as dvh handles this */
}
```

## References
- [CSS Working Group: Viewport Units](https://drafts.csswg.org/css-values-4/#viewport-relative-lengths)
- [Invisible Viewport: Address bar height on mobile](https://webkit.org/blog/13936/invisible-viewport-address-bar-height-on-mobile/)
- [Can I use: dvh](https://caniuse.com/mdn-css_units_dvh)
