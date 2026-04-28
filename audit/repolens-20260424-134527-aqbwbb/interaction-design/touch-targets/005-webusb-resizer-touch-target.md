---
title: "[MEDIUM] WebUSB serial example has 5px resizer - too small for touch"
severity: MEDIUM
domain: managed_components/tinyusb
lens: touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The WebUSB serial example (`managed_components/espressif__tinyusb/examples/device/webusb_serial/website/style.css`) includes a column resizer that is only 5px wide, making it nearly impossible to grab with a finger on touch devices.

**Location:** `managed_components/espressif__tinyusb/examples/device/webusb_serial/website/style.css:206-209`

## Impact
- **Touch Devices:** 5px width is far below the 44px minimum touch target size - users cannot grab the resizer with a finger
- **Usability:** Column resizing feature is effectively broken on tablets and touch laptops
- **Accessibility:** Fails WCAG 2.5.8 Target Size (Minimum) for touch interfaces

## Evidence
The resizer styles (lines 206-209):

```css
.resizer {
  width: 5px;
  background-color: #ccc;
  cursor: col-resize;
  height: 100%;
}
```

A 5px wide element requires precise mouse cursor alignment. On touch devices, the average finger pad is 20-30px, making this resizer virtually untappable.

## Recommended Fix
Increase the resizer width and add a larger touch target with transparent padding:

```css
.resizer {
  width: 12px; /* Visual width */
  background-color: #ccc;
  cursor: col-resize;
  height: 100%;
  position: relative;
}

/* Add larger touch target for touch devices */
@media (any-pointer: coarse) {
  .resizer {
    width: 24px; /* Larger for touch */
    background-color: transparent; /* Hide wider area */
  }
  
  .resizer::after {
    content: '';
    position: absolute;
    left: 50%;
    transform: translateX(-50%);
    width: 12px; /* Original visual width */
    height: 100%;
    background-color: #ccc;
  }
}
```

Alternative approach with pseudo-element for click area:

```css
.resizer {
  width: 8px;
  background-color: #ccc;
  cursor: col-resize;
  height: 100%;
  position: relative;
}

.resizer::before {
  content: '';
  position: absolute;
  top: 0;
  left: -6px; /* Extend hit area */
  width: 16px; /* Total: 8px + 6px + 6px = 20px */
  height: 100%;
  background: transparent;
  cursor: col-resize;
}

@media (any-pointer: coarse) {
  .resizer {
    width: 12px;
  }
  
  .resizer::before {
    left: -12px; /* Total: 12px + 12px + 12px = 36px */
    width: 36px;
  }
}
```

## References
- [WCAG 2.5.8 Target Size (Minimum)](https://www.w3.org/WAI/WCAG21/Understanding/target-size.html)
- [Material Design Touch Targets](https://m3.material.io/foundations/interactions/touch-interaction/touch-targets)
- [MDN: CSS Media Queries - Pointer](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/any-pointer)
