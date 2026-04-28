---
title: "[LOW] Web flasher uses text-align: center without considering RTL alignment needs"
severity: LOW
domain: web-flasher
lens: rtl-layout
labels:
  - "audit:adaptive-ux/rtl-layout"
---

## Summary

The web flasher (`web-flasher/index.html`) uses `text-align: center` in multiple places. While center alignment works in both directions, other alignment values like `text-align: left` should be `text-align: start` for RTL support.

Additionally, the inline text content in lines 223-227 contains mixed LTR technical terms (TROPIC01, ESP32-S3, CDC Badge v1.0) that might benefit from `dir="auto"` or `<bdi>` elements for proper bidirectional text handling in RTL contexts.

## Impact

- `text-align: center` works correctly in RTL, but using `text-align: start` or `text-align: end` explicitly signals intent
- Mixed LTR/RTL inline content could shape incorrectly in RTL languages
- Technical terms might break or display with wrong direction in Arabic/Hebrew contexts

## Evidence

**File**: `web-flasher/index.html`

**Lines 44, 144, 176, 197** - Center alignment:
```css
text-align: center;
```

**Lines 223-227** - Mixed content:
```html
<p style="font-size: 0.9rem; color: var(--text-muted);">
  This firmware is designed exclusively for the <strong style="color: var(--text);">TROPIC01-enhanced CDC Badge v1.0 / v1.1</strong> based on ESP32-S3.
  Do not flash on other devices.
</p>
```

## Recommended Fix

For future RTL support, consider:

1. Use `text-align: start` or `text-align: end` instead of `text-align: left/right` when alignment needs to flip
2. For mixed LTR/RTL content, add `dir="auto"` to containers:
```html
<p dir="auto" style="font-size: 0.9rem; color: var(--text-muted);">
  This firmware is designed exclusively for the <strong dir="ltr">TROPIC01-enhanced CDC Badge v1.0 / v1.1</strong> based on ESP32-S3.
  Do not flash on other devices.
</p>
```

Using `<bdi>` (bidirectional isolate) for technical terms:
```html
<bdi>TROPIC01-enhanced CDC Badge v1.0 / v1.1</bdi>
<bdi>ESP32-S3</bdi>
```

## References

- [MDN - dir="auto"](https://developer.mozilla.org/en-US/docs/Web/HTML/Global_attributes/dir#auto)
- [MDN - bdi element](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/bdi)
- [Unicode Bidirectional Algorithm](https://www.unicode.org/reports/tr9/)
