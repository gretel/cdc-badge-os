---
title: "[MEDIUM] Web Flasher missing dynamic dir attribute for RTL support"
severity: MEDIUM
domain: web-flasher
lens: rtl-layout
labels:
  - "audit:adaptive-ux/rtl-layout"
---

## Summary

The web flasher (`web-flasher/index.html`) has a hardcoded `<html lang="en">` without:
1. A mechanism to dynamically set `dir="rtl"` based on locale
2. Any CSS `[dir="rtl"]` selector overrides for RTL layout adjustments
3. Missing `lang` attribute updates when locale changes

## Impact

Without RTL infrastructure:
- Browsers cannot apply RTL text shaping rules
- Assistive technologies won't know to use RTL reading order
- User-generated content with mixed LTR/RTL text will have incorrect bidirectional handling
- No foundation exists for adding RTL language support in the future

## Evidence

**File**: `web-flasher/index.html`

**Line 2** - Hardcoded English language attribute:
```html
<html lang="en">
```

No `dir` attribute present, and no JavaScript or CSS to handle RTL:
- No `[dir="rtl"]` CSS selectors anywhere in the stylesheet
- No mechanism to detect and apply user's preferred direction
- No `dir="auto"` on content areas that might contain user-generated text

## Recommended Fix

1. Add a `dir` attribute to the `<html>` element:
```html
<html lang="en" dir="ltr">
```

2. Add a simple mechanism to detect and apply locale direction. For a static page like this, you can:
   - Detect browser language and set direction via JavaScript on page load:
```javascript
<script>
  const rtlLanguages = ['ar', 'he', 'fa', 'ur'];
  const userLang = navigator.language || navigator.userLanguage;
  const dir = rtlLanguages.some(lang => userLang.startsWith(lang)) ? 'rtl' : 'ltr';
  document.documentElement.setAttribute('dir', dir);
  document.documentElement.setAttribute('lang', userLang.split('-')[0]);
</script>
```

3. Add RTL-specific CSS overrides where needed:
```css
[dir="rtl"] .steps li {
  /* Adjust step counter for RTL */
  padding: 0.75rem 3rem 0.75rem 0.75rem;
}

[dir="rtl"] .steps li::before {
  inset-inline-start: auto;
  inset-inline-end: 0.75rem;
}
```

## References

- [W3C i18n - Direction](https://www.w3.org/International/articles/html-html5dir/)
- [MDN - dir attribute](https://developer.mozilla.org/en-US/docs/Web/HTML/Global_attributes/dir)
- [CSS Writing Modes Spec](https://www.w3.org/TR/css-writing-modes-3/)
