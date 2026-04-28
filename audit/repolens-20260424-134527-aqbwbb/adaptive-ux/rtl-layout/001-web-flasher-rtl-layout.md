---
title: "[MEDIUM] Web Flasher uses physical CSS properties instead of logical properties"
severity: MEDIUM
domain: web-flasher
lens: rtl-layout
labels:
  - "audit:adaptive-ux/rtl-layout"
---

## Summary

The web flasher (`web-flasher/index.html`) uses physical CSS directional properties that will break in RTL layouts:

1. **Line 106**: `left: 0.75rem;` - hardcoded positioning for step counter
2. **Line 188**: `padding-left: 1.25rem;` - hardcoded left padding for requirements list

These properties do not automatically flip when `dir="rtl"` is applied.

## Impact

When the interface is displayed in RTL languages (Arabic, Hebrew, etc.):
- The numbered step counter (line 106) will appear on the wrong side, breaking visual hierarchy
- The requirements list bullets (line 188) will be misaligned with their text
- Overall layout will feel "wrong" to RTL users as visual weight is on the incorrect side

## Evidence

**File**: `web-flasher/index.html`

**Line 106** - Step counter positioning:
```css
.steps li::before {
  content: counter(step);
  position: absolute;
  left: 0.75rem;  /* Should be inset-inline-start */
  top: 0.75rem;
  ...
}
```

**Line 188** - List padding:
```css
.requirements ul {
  list-style: disc;
  padding-left: 1.25rem;  /* Should be padding-inline-start */
  margin-top: 0.5rem;
}
```

**Line 24** also uses `margin: 0; padding: 0;` which is fine, but other directional properties throughout the file use physical values.

## Recommended Fix

Replace physical directional properties with logical equivalents:

1. Change `left: 0.75rem;` to `inset-inline-start: 0.75rem;`
2. Change `padding-left: 1.25rem;` to `padding-inline-start: 1.25rem;`

The updated CSS should be:

```css
.steps li::before {
  content: counter(step);
  position: absolute;
  inset-inline-start: 0.75rem;
  top: 0.75rem;
  ...
}

.requirements ul {
  list-style: disc;
  padding-inline-start: 1.25rem;
  margin-top: 0.5rem;
}
```

## References

- [CSS Logical Properties - MDN](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_logical_properties_and_values)
- [W3C CSS Logical Properties Spec](https://www.w3.org/TR/css-logical-1/)
- [Inclusive Components - RTL](https://inclusive-components.design/)
