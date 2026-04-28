---
title: "[MEDIUM] Notice icon lacks accessible name for screen readers"
severity: MEDIUM
domain: frontend
lens: a11y
labels:
  - "audit:frontend/accessibility"
---

## Summary
The warning/notice icon (using `!` character) in the notice section has no accessible name. Screen reader users will not know that this is an important visual indicator for the warning message.

**Location:** `web-flasher/index.html`, lines 140-141 and line 265

```html
<div class="notice">
  <span class="notice-icon">!</span>
  <span>
    The display will not react and the device will appear to be turned off.
    This is expected &mdash; the badge is now in bootloader mode.
  </span>
</div>
```

```css
.notice-icon {
  flex-shrink: 0;
  font-size: 1.1rem;
  line-height: 1.4;
  /* No aria-label or role defined */
}
```

## Impact
- **Screen reader users miss the visual cue** - The icon conveys important information (warning/caution)
- **WCAG 2.1 Success Criterion 1.1.1 (Non-text Content)** - All non-text content needs a text alternative
- The icon is decorative but also functional as a warning indicator - ambiguity needs resolution
- Users relying on assistive technology won't know this is a warning notice

## Evidence
- Line 140-141: Definition of `.notice-icon` class
- Line 265: Usage of the icon with just `!` character
- The icon has no `aria-label`, `aria-labelledby`, `role="img"`, or `alt` attribute
- No `role="status"` or `role="alert"` on the parent notice div

## Recommended Fix
Add an accessible name to the icon and improve the notice structure:

```html
<div class="notice" role="status">
  <span class="notice-icon" role="img" aria-label="Note">!</span>
  <span>
    The display will not react and the device will appear to be turned off.
    This is expected &mdash; the badge is now in bootloader mode.
  </span>
</div>
```

Or use `aria-hidden` if the icon is purely decorative and the text content is sufficient:

```html
<div class="notice" role="status">
  <span class="notice-icon" aria-hidden="true">!</span>
  <span>
    The display will not react and the device will appear to be turned off.
    This is expected &mdash; the badge is now in bootloader mode.
  </span>
</div>
```

## References
- [WCAG 2.1 Success Criterion 1.1.1 Non-text Content](https://www.w3.org/TR/WCAG21/#non-text-content)
- [MDN: aria-label attribute](https://developer.mozilla.org/en-US/docs/Web/Accessibility/ARIA/Attributes/aria-label)
- [WAI-ARIA Authoring Practices - Status](https://www.w3.org/WAI/ARIA/apg/patterns/status/)
