---
title: "[LOW] Doxygen docs missing touch-specific media queries for hover interactions"
severity: LOW
domain: documentation
lens: touch-targets
labels:
  - "audit:interaction-design/touch-targets"
---

## Summary
The Doxygen documentation CSS (`third_party/libtropic/docs/doxygen/html/doxygen-awesome.css`) uses 34 `:hover` selectors but lacks `@media (hover: none)` or `@media (pointer: coarse)` queries to provide touch-friendly alternatives for devices without precise pointing.

**Location:** `third_party/libtropic/docs/doxygen/html/doxygen-awesome.css` (throughout)

## Impact
- **Mobile UX:** Hover-only interactions (dropdown menus, tooltips, code copy buttons) may be difficult or impossible to trigger on touch devices
- **Accessibility:** Users on tablets and touch laptops may miss interactive elements that only appear on hover
- **Discoverability:** Features like the code snippet copy button (`.doxygen-awesome-fragment-wrapper:hover doxygen-awesome-fragment-copy-button`) require hover to appear

## Evidence
The CSS uses hover for multiple interactions:

1. **Code copy button visibility** (line 2424):
```css
.doxygen-awesome-fragment-wrapper:hover doxygen-awesome-fragment-copy-button, doxygen-awesome-fragment-copy-button.success {
    opacity: .28;
}
```

2. **Navigation menu hover states** (34 total `:hover` occurrences):
```css
.sm-dox a:hover { ... }
.navpath li.navelem a:hover { ... }
#main-menu > li:hover > ul { ... }
```

3. **No touch-capability queries found:**
```
grep "@media (hover: none)\|@media (pointer: coarse)" → no results
```

## Recommended Fix
Add touch-specific media queries to improve mobile experience:

### 1. Increase touch target size on coarse pointer devices:
```css
@media (any-pointer: coarse) {
    doxygen-awesome-fragment-copy-button {
        width: 44px;
        height: 44px;
    }
    
    .navpath li.navelem a {
        padding: 10px 15px; /* Larger tap area */
    }
}
```

### 2. Make hover-only elements always visible or tap-toggleable:
```css
@media (hover: none) {
    /* Always show copy button on touch devices */
    doxygen-awesome-fragment-copy-button {
        opacity: 1;
    }
    
    /* Larger touch targets for navigation */
    #main-menu {
        font-size: 16px; /* Larger text */
    }
}
```

### 3. Add `touch-action` for better tap response:
```css
@media (any-pointer: coarse) {
    a, button, doxygen-awesome-fragment-copy-button {
        touch-action: manipulation;
    }
}
```

## References
- [CSS Media Queries: Hover](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/hover)
- [CSS Media Queries: Pointer](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/any-pointer)
- [W3C Hover Media Feature](https://www.w3.org/TR/mediaqueries-4/#hover)
- [Doxygen Awesome CSS GitHub](https://github.com/jothepro/doxygen-awesome-css)
