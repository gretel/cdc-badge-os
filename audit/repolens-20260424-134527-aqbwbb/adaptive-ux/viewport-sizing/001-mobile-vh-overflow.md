---
title: "[MEDIUM] Legacy 100vh usage in web-flasher causes overflow on mobile browsers"
severity: MEDIUM
domain: adaptive-ux/viewport-sizing
lens: viewport-sizing
labels:
  - "audit:adaptive-ux/viewport-sizing"
---

## Summary
The CDC Badge OS web-flasher (`web-flasher/index.html`) uses `min-height: 100vh` on the body element (line 31), which causes content to overflow when viewed on mobile browsers where the address bar consumes vertical space.

**File:** `web-flasher/index.html`  
**Line:** 31  
**CSS Property:** `min-height: 100vh`

```css
body {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
  background: var(--bg);
  color: var(--text);
  line-height: 1.6;
  min-height: 100vh;  /* Line 31 - problematic on mobile */
  display: flex;
  flex-direction: column;
  align-items: center;
}
```

## Impact
- On mobile devices (iOS Safari, Android Chrome), the URL/address bar is included in the viewport height calculation
- Content that should fit the screen extends below the fold, hidden behind the address bar
- Users must scroll to see content that appears to be "full screen"
- Poor user experience, especially for a flasher tool that should be immediately usable

## Evidence
The CSS uses the traditional `vh` unit which calculates based on the full viewport height including browser chrome. Modern mobile browsers dynamically show/hide the address bar, but `100vh` doesn't adapt to this.

From `web-flasher/index.html:31`:
```css
min-height: 100vh;
```

## Recommended Fix
Replace `100vh` with dynamic viewport units (`dvh`) as progressive enhancement:

```css
body {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
  background: var(--bg);
  color: var(--text);
  line-height: 1.6;
  min-height: 100vh;
  min-height: 100dvh;  /* Dynamic viewport height for mobile browsers */
  display: flex;
  flex-direction: column;
  align-items: center;
}
```

**Browser Support:**
- `dvh` is supported in all modern mobile browsers (Chrome 115+, Safari 17.4+, Firefox 120+)
- The fallback `100vh` ensures graceful degradation for older browsers

## References
- [CSS Working Group: Viewport Units](https://drafts.csswg.org/css-values-4/#viewport-relative-lengths)
- [Invisible Viewport: Address bar height on mobile](https://webkit.org/blog/13936/invisible-viewport-address-bar-height-on-mobile/)
- [Can I use: dvh](https://caniuse.com/mdn-css_units_dvh)
