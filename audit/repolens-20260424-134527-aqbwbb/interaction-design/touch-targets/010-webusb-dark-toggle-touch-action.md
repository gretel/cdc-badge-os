---
title: "[LOW] WebUSB dark mode toggle button lacks touch-action CSS"
severity: LOW
domain: managed_components/tinyusb
lens: touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The dark mode toggle button in the WebUSB serial example (`managed_components/espressif__tinyusb/examples/device/webusb_serial/website/style.css`) lacks `touch-action: manipulation` CSS, which could result in a 300ms tap delay on mobile devices.

**Location:** `managed_components/espressif__tinyusb/examples/device/webusb_serial/website/style.css:22-29`

## Impact
- **Tap Delay:** Without `touch-action: manipulation`, mobile browsers may wait 300ms to distinguish between tap and double-tap
- **Perceived Responsiveness:** Users experience a noticeable lag between tap and state change
- **User Experience:** Interactive elements feel sluggish on mobile devices

## Evidence
The dark mode toggle button in the header section (lines 22-29):

```css
/* Header row styling */
.header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0.5em 1em;
  gap: 1em;
  flex-shrink: 0;
}
```

The button (`.btn-theme`) uses default touch behavior. While the button has reasonable size (padding: 0.5rem 1rem), it lacks explicit touch optimization.

## Recommended Fix
Add `touch-action: manipulation` to interactive buttons:

```css
.btn-theme {
  background-color: #6b6b6b;
  color: #fff;
  touch-action: manipulation; /* Eliminate 300ms tap delay */
}

/* Apply to all interactive buttons */
.btn {
  padding: 0.5rem 1rem;
  font-size: 1rem;
  border: none;
  border-radius: 0.3rem;
  cursor: pointer;
  touch-action: manipulation; /* Eliminate 300ms tap delay */
}
```

Alternatively, apply globally to reduce tap delay across the entire document:

```css
/* Add to base styles */
body {
  /* ... existing styles ... */
  touch-action: manipulation; /* Apply to entire document */
}
```

Note: `touch-action: manipulation` disables double-tap-to-zoom but allows pinch-to-zoom. For most interactive UI, this is a safe and recommended optimization.

## References
- [MDN: touch-action CSS property](https://developer.mozilla.org/en-US/docs/Web/CSS/touch-action)
- [Eliminate the 300ms tap delay](https://developer.chrome.com/blog/300ms-tap-delay-gone-away/)
- [CSS Touch Action Module Level 1](https://www.w3.org/TR/css-touch-action-1/)
