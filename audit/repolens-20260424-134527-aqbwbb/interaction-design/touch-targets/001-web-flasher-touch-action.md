---
title: "[LOW] Web flasher missing touch-action CSS for mobile tap optimization"
severity: LOW
domain: web-flasher
lens: touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The web flasher (`web-flasher/index.html`) lacks `touch-action` CSS declarations on interactive elements, specifically the `esp-web-install-button` component. This can result in a 300ms tap delay on some mobile browsers and may trigger unintended browser gestures (scroll, zoom) during button taps.

**Location:** `web-flasher/index.html:150-165` (button styles)

## Impact
- **User Experience:** ~300ms tap delay on mobile browsers without fast-click detection
- **Gesture Conflicts:** Potential scroll/zoom conflicts when tapping the install button on small screens
- **Mobile Responsiveness:** Slightly slower perceived responsiveness on touch devices

## Evidence
The button styles in `web-flasher/index.html` (lines 150-165):

```css
esp-web-install-button button {
  background: var(--accent);
  color: var(--bg);
  border: none;
  border-radius: 0.5rem;
  padding: 0.85rem 2rem;
  font-size: 1rem;
  font-weight: 600;
  cursor: pointer;
  transition: background 0.15s;
}
```

Missing `touch-action: manipulation` declaration which eliminates 300ms delay and constrains browser gestures.

## Recommended Fix
Add `touch-action: manipulation` to the button styles:

```css
esp-web-install-button button {
  background: var(--accent);
  color: var(--bg);
  border: none;
  border-radius: 0.5rem;
  padding: 0.85rem 2rem;
  font-size: 1rem;
  font-weight: 600;
  cursor: pointer;
  transition: background 0.15s;
  touch-action: manipulation; /* Add this line */
}
```

This tells the browser to handle touch events for manipulation (tap, double-tap) without waiting for potential zoom gestures, eliminating the 300ms delay.

## References
- [MDN: touch-action](https://developer.mozilla.org/en-US/docs/Web/CSS/touch-action)
- [CSS Touch Action Level 1 Spec](https://www.w3.org/TR/touch-action/)
- Google Developers: [Removing the 300ms tap delay](https://developers.google.com/web/updates/2013/12/300ms-tap-delay-gone-away)
