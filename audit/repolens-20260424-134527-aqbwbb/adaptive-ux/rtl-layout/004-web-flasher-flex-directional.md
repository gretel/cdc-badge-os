---
title: "[LOW] Web Flasher uses physical flexbox alignment for directional icons"
severity: LOW
domain: web-flasher
lens: rtl-layout
labels:
  - "audit:adaptive-ux/rtl-layout"
---

## Summary
The web-flasher uses physical flexbox alignment and directional visual elements that would need adjustment for RTL:

**Line 126-136**: Notice icon uses `flex-shrink: 0` with implicit left positioning
**Line 226-228**: Image container uses `flex-shrink: 0` with left-aligned image

While `flex-direction: row` naturally adapts to RTL, the visual hierarchy (icon/image on left) may not be appropriate for RTL layouts where these elements should appear on the right.

## Impact
In RTL layouts:
- Notice icons would ideally appear on the right side for better visual flow
- The badge image might need right-alignment in the info card
- Arrow indicators (if added) would need mirroring

## Evidence
```css
/* Line 126-136 - Notice with implicit left icon */
.notice {
  display: flex;
  gap: 0.5rem;
  align-items: flex-start;
  /* Icon appears on left by default flex behavior */
}

/* Line 226-228 - Image on left in card */
.card {
  /* Image appears on left due to flex order */
  img {
    flex-shrink: 0;  /* Fixed size, but position is LTR */
  }
}
```

## Recommended Fix
Add `[dir="rtl"]` overrides for directional elements:

```css
/* RTL override for notice icon */
[dir="rtl"] .notice {
  flex-direction: row-reverse;
}

/* RTL override for image card */
[dir="rtl"] .card {
  flex-direction: row-reverse;
}

/* Or use flex-order with logical properties */
[dir="rtl"] .notice-icon {
  order: 2;  /* Move icon to right */
}
```

Alternatively, use CSS logical properties for flexbox:
```css
.notice {
  display: flex;
  flex-direction: row;  /* Already RTL-aware */
  justify-content: flex-start;  /* Use start instead of left */
}
```

## References
- [MDN: Flexbox and RTL](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_Flexible_Box_Model/Ordering_Flex_Items)
- [CSS Flexbox Logical Properties](https://www.w3.org/TR/css-flexbox-1/#box-order)
