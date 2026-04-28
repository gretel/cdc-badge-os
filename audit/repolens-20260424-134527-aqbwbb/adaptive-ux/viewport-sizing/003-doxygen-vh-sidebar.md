---
title: "[LOW] Doxygen documentation uses 100vh for fixed sidebar on mobile"
severity: LOW
domain: adaptive-ux/viewport-sizing
lens: viewport-sizing
labels:
  - "audit:adaptive-ux/viewport-sizing"
---

## Summary
The libtropic Doxygen documentation (`third_party/libtropic/docs/doxygen/html/doxygen-awesome-sidebar-only.css`) uses `calc(100vh - ...)` expressions for fixed sidebar heights (lines 40, 61, 98), causing content to extend below the fold on mobile devices.

**Files:**
- `third_party/libtropic/docs/doxygen/html/doxygen-awesome-sidebar-only.css`
- `third_party/libtropic/docs/doxygen/html/doxygen-awesome.css`

**Lines:** 40, 61, 98 (sidebar-only)

```css
/* Line 40 */
--toc-max-height: calc(100vh - 2 * var(--spacing-medium) - 25px);

/* Line 61 */
height: calc(100vh - var(--top-height)) !important;

/* Line 98 */
height: calc(100vh - 31px) !important;
```

## Impact
- Documentation sidebar extends beyond visible area on mobile
- Table of contents may be partially hidden behind address bar
- Users need to scroll to access all navigation items
- Less critical since this is third-party documentation, not core application

## Evidence
From `third_party/libtropic/docs/doxygen/html/doxygen-awesome-sidebar-only.css`:

```css
/* Line 40 */
--toc-max-height: calc(100vh - 2 * var(--spacing-medium) - 25px);

/* Lines 60-62 */
#nav-tree, #side-nav {
    height: calc(100vh - var(--top-height)) !important;
}

/* Lines 97-99 */
#doc-content {
    height: calc(100vh - 31px) !important;
    ...
}
```

From `third_party/libtropic/docs/doxygen/html/doxygen-awesome.css` (lines 84, 749, 757, 1970, 1984):
```css
/* Line 84 */
--toc-max-height: calc(100vh - 2 * var(--spacing-medium) - 85px);

/* Line 749 */
width: calc(100vw - 30px);

/* Line 757 */
width: calc(100vw - 110px);

/* Lines 1970, 1984 */
width: calc(100vw - 2 * var(--spacing-large));
```

## Recommended Fix
Since this is third-party documentation CSS, the fix involves updating the Doxygen Awesome CSS theme or applying overrides:

**Option 1: Update to latest Doxygen Awesome CSS**
```bash
# Download latest version with mobile improvements
curl -o docs/doxygen/html/doxygen-awesome.css \
  https://raw.githubusercontent.com/jothepro/doxygen-awesome-css/main/doxygen-awesome.css
```

**Option 2: Apply CSS overrides in a custom stylesheet**
```css
@media screen and (max-width: 767px) {
    #nav-tree, #side-nav {
        height: calc(100dvh - var(--top-height)) !important;
    }
    
    #doc-content {
        height: calc(100dvh - 31px) !important;
    }
    
    --toc-max-height: calc(100dvh - 2 * var(--spacing-medium) - 25px);
}
```

## References
- [Doxygen Awesome CSS GitHub](https://github.com/jothepro/doxygen-awesome-css)
- [CSS Working Group: Viewport Units](https://drafts.csswg.org/css-values-4/#viewport-relative-lengths)
- [Can I use: dvh](https://caniuse.com/mdn-css_units_dvh)
