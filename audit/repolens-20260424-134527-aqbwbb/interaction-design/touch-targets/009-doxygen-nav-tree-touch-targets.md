---
title: "[LOW] Doxygen nav-tree labels and arrows undersized for touch"
severity: LOW
domain: documentation
lens: touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The navigation tree labels and arrows in Doxygen documentation (`doxygen_output/html/navtree.css`) have minimal padding and small dimensions, resulting in touch targets that are undersized for comfortable tapping on mobile devices.

**Location:** `doxygen_output/html/navtree.css:42-56, 1509-1540`

## Impact
- **Mobile Usability:** Navigation tree items with 2px padding and 22px height are difficult to tap accurately on touch devices
- **Accessibility:** Fails WCAG 2.5.8 Target Size (Minimum) guidelines - requires 44px minimum
- **User Experience:** Navigation is a primary interaction pattern; undersized targets cause friction in browsing documentation

## Evidence
The navigation tree label styles in `navtree.css` (lines 42-56):

```css
#nav-tree .label {
  margin:0px;
  padding:0px;
  font: 12px var(--font-family-nav);
  line-height: 22px;
}

#nav-tree .label a {
  padding:2px;
}
```

The `.label` has 0 padding and 22px line-height. The link inside has only 2px padding, creating a touch target of approximately 26px height × variable width.

The arrow styles (lines 1509-1540):

```css
.arrow {
  color: var(--nav-background-color);
  -webkit-user-select: none;
  -khtml-user-select: none;
  -moz-user-select: none;
  -ms-user-select: none;
  user-select: none;
  cursor: pointer;
  font-size: 80%;
  display: inline-block;
  width: 16px;
  height: 14px;
  transition: opacity 0.3s ease;
}

span.arrowhead {
  position: relative;
  padding: 0;
  margin: 0 0 0 2px;
  display: inline-block;
  width: 5px;
  height: 5px;
  border-right: 2px solid var(--nav-arrow-color);
  border-bottom: 2px solid var(--nav-arrow-color);
  transform: rotate(-45deg);
  transition: transform 0.3s ease;
}
```

The `.arrow` is 16x14px and `.arrowhead` is 5x5px - both far below the 44px minimum touch target size.

## Recommended Fix
Increase padding and add minimum dimensions for touch targets:

```css
#nav-tree .label {
  margin:0px;
  padding:0px;
  font: 12px var(--font-family-nav);
  line-height: 22px;
}

#nav-tree .label a {
  padding:8px 4px; /* Increased from 2px */
  display: inline-block;
  min-height: 44px; /* Ensure minimum touch target */
  line-height: 22px;
}

/* Increase for coarse pointers */
@media (any-pointer: coarse) {
  #nav-tree .label a {
    padding:12px 6px;
    min-height: 48px;
  }
}
```

For the arrow, increase the clickable area:

```css
.arrow {
  color: var(--nav-background-color);
  -webkit-user-select: none;
  -khtml-user-select: none;
  -moz-user-select: none;
  -ms-user-select: none;
  user-select: none;
  cursor: pointer;
  font-size: 80%;
  display: inline-block;
  width: 16px;
  height: 14px;
  transition: opacity 0.3s ease;
  /* Add larger touch target */
  padding: 12px; /* Creates ~40px hit area */
  margin: -12px; /* Compensate for padding */
}

span.arrowhead {
  position: relative;
  padding: 0;
  margin: 0 0 0 2px;
  display: inline-block;
  width: 5px;
  height: 5px;
  border-right: 2px solid var(--nav-arrow-color);
  border-bottom: 2px solid var(--nav-arrow-color);
  transform: rotate(-45deg);
  transition: transform 0.3s ease;
}

/* Increase for coarse pointers */
@media (any-pointer: coarse) {
  .arrow {
    padding: 14px;
    margin: -14px;
  }
}
```

## References
- [WCAG 2.5.8 Target Size (Minimum)](https://www.w3.org/WAI/WCAG21/Understanding/target-size.html)
- [Material Design Touch Targets](https://m3.material.io/foundations/interactions/touch-interaction/touch-targets)
- [MDN: CSS Media Queries - Pointer](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/any-pointer)
