---
title: "[MEDIUM] Web Flasher missing RTL infrastructure and hardcoded directional CSS"
severity: MEDIUM
domain: web-flasher
lens: rtl-layout
labels:
  - "audit:adaptive-ux/rtl-layout"
---

## Summary
The web-flasher `index.html` has the following RTL issues:
- **Line 2**: `<html lang="en">` lacks a `dir` attribute to specify text direction
- **Line 106**: Hardcoded `left: 0.75rem;` for step indicator positioning (`.steps li::before`)
- **Line 188**: Hardcoded `padding-left: 1.25rem;` for requirements list (`.requirements ul`)

## Impact
When `dir="rtl"` is applied:
- The step number indicators will appear on the wrong side (left instead of right)
- List bullets will be positioned incorrectly
- No mechanism exists to dynamically set direction based on locale

## Evidence
```css
/* Line 106 - Step indicator positioned with physical left */
.steps li::before {
  content: counter(step);
  position: absolute;
  left: 0.75rem;  /* Should use inset-inline-start */
  top: 0.75rem;
  ...
}

/* Line 188 - List padding uses physical left */
.requirements ul {
  list-style: disc;
  padding-left: 1.25rem;  /* Should use padding-inline-start */
  margin-top: 0.5rem;
}
```

HTML structure (line 2):
```html
<html lang="en">  <!-- Missing dir attribute -->
```

## Recommended Fix
1. Add `dir` attribute to `<html>` element with dynamic binding to locale:
   ```html
   <html lang="en" dir="ltr">  <!-- Or dir="rtl" for RTL languages -->
   ```

2. Replace physical CSS properties with logical equivalents:
   ```css
   .steps li::before {
     left: 0.75rem;           /* OLD */
     inset-inline-start: 0.75rem;  /* NEW */
   }

   .requirements ul {
     padding-left: 1.25rem;   /* OLD */
     padding-inline-start: 1.25rem;  /* NEW */
   }
   ```

3. Consider adding a language/direction switcher for multi-locale support.

## References
- [MDN: Logical Properties](https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_logical_properties_and_values)
- [W3C: CSS Writing Modes](https://www.w3.org/TR/css-writing-modes-3/)
