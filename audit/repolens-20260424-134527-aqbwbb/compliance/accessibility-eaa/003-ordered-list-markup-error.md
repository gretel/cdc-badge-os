---
title: "[LOW] Ordered list structure error in instructions"
severity: LOW
domain: web-flasher
lens: accessibility-eaa
labels:
  - "wcag-2.1"
  - "semantic-html"
---

## Summary
In `web-flasher/index.html` line 248, a `<li>` element is placed outside of a parent `<ol>` or `<ul>` container, creating invalid HTML structure.

## Impact
Invalid list structure can cause issues for:
- Screen readers that rely on proper semantic markup
- Users navigating via keyboard
- Accessibility evaluation tools

The list items for "Enter Bootloader Mode" (lines 243-248) use `<ol class="steps">` but line 248 has a `<li>` element that appears to be a continuation of the ordered list but is not properly nested.

## Evidence
File: `web-flasher/index.html` (lines 243-248):
```html
<ol class="steps">
  <li>Press and <strong>hold FLASH</strong></li>
  <li>While holding FLASH, press <strong>RESET</strong> once and release it</li>
  <li>Release <strong>FLASH</strong></li>
<li>After flashing, press <strong>RESET</strong> once to start the new firmware</li>
</ol>
```

Line 248 has inconsistent indentation and the closing `</ol>` tag is on the same line as the last `<li>`, suggesting a markup error.

## Recommended Fix
Fix the list structure by ensuring all `<li>` elements are properly nested:

```html
<ol class="steps">
  <li>Press and <strong>hold FLASH</strong></li>
  <li>While holding FLASH, press <strong>RESET</strong> once and release it</li>
  <li>Release <strong>FLASH</strong></li>
  <li>After flashing, press <strong>RESET</strong> once to start the new firmware</li>
</ol>
```

## References
- [HTML5 List Specification](https://html.spec.whatwg.org/multipage/grouping-content.html#the-ol-element)
- [WCAG 2.1 SC 1.3.1 Info and Relationships](https://www.w3.org/WAI/WCAG21/Understanding/info-and-relationships.html)
