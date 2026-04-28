---
title: "[MEDIUM] Doxygen search close icon undersized (11x11px)"
severity: MEDIUM
domain: documentation
lens: touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The search box close icon in the Doxygen documentation (`doxygen_output/html/search/search.css`) is only 11x11px, far below the recommended 44x44px minimum touch target size for mobile devices.

**Location:** `doxygen_output/html/search/search.css:87-113`

## Impact
- **Mobile Usability:** 11x11px icon is extremely difficult to tap accurately on touch devices
- **Accessibility:** Fails WCAG 2.5.8 Target Size (Minimum) guidelines - requires 44x44px minimum
- **User Experience:** Users on tablets or mobile devices will struggle to clear the search field, leading to frustration and potential abandonment
- **High Priority Control:** Close/clear buttons are critical for user flow - missing targets here cause more friction than decorative elements

## Evidence
The close icon styles in `search/search.css` (lines 87-113):

```css
.close-icon {
  width: 11px;
  height: 11px;
  background-color: var(--search-close-icon-bg-color);
  border-radius: 50%;
  position: relative;
  display: flex;
  justify-content: center;
  align-items: center;
  box-sizing: content-box;
}

.close-icon:before,
.close-icon:after {
  content: '';
  position: absolute;
  width: 7px;
  height: 1px;
  background-color: var(--search-close-icon-fg-color);
}

.close-icon:before {
  transform: rotate(45deg);
}

.close-icon:after {
  transform: rotate(-45deg);
}
```

The close icon is 11x11px with 1px thick lines. This is less than 1/4 of the recommended 44px minimum touch target size.

## Recommended Fix
Add a larger touch target using a pseudo-element or wrapper while keeping the visual icon small:

```css
.close-icon {
  width: 11px;
  height: 11px;
  background-color: var(--search-close-icon-bg-color);
  border-radius: 50%;
  position: relative;
  display: flex;
  justify-content: center;
  align-items: center;
  box-sizing: content-box;
  /* Add larger touch target area */
  padding: 16px; /* Creates 43px total hit area (11px + 2*16px) */
  cursor: pointer;
}

/* Optionally make it even larger on coarse pointers */
@media (any-pointer: coarse) {
  .close-icon {
    padding: 18px; /* Creates 47px total hit area */
  }
}

.close-icon:before,
.close-icon:after {
  content: '';
  position: absolute;
  width: 7px;
  height: 1px;
  background-color: var(--search-close-icon-fg-color);
  z-index: 1; /* Ensure cross marks stay on top */
}

.close-icon:before {
  transform: rotate(45deg);
}

.close-icon:after {
  transform: rotate(-45deg);
}
```

Alternative approach using a larger clickable wrapper:

```css
.close-icon-wrapper {
  position: relative;
  width: 44px;
  height: 44px;
  display: flex;
  justify-content: center;
  align-items: center;
  cursor: pointer;
}

.close-icon {
  width: 11px;
  height: 11px;
  background-color: var(--search-close-icon-bg-color);
  border-radius: 50%;
  position: relative;
  display: flex;
  justify-content: center;
  align-items: center;
  box-sizing: content-box;
  z-index: 1;
}

/* ... rest of close-icon styles ... */
```

## References
- [WCAG 2.5.8 Target Size (Minimum)](https://www.w3.org/WAI/WCAG21/Understanding/target-size.html)
- [Material Design Touch Targets](https://m3.material.io/foundations/interactions/touch-interaction/touch-targets)
- [MDN: CSS Media Queries - Pointer](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/any-pointer)
