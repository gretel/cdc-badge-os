---
title: "[MEDIUM] Inline style attributes bypass stylesheet cascade in web-flasher"
severity: MEDIUM
domain: design-system
lens: css-architecture
labels:
  - "inline-styles"
  - "maintainability"
---

## Summary
The `web-flasher/index.html` file contains **7 inline `style` attributes** scattered across lines 226-270, mixing inline styles with the embedded stylesheet. This creates inconsistent styling patterns and makes maintenance harder.

**Locations:**
- Line 226: `<div class="card" style="display: flex; gap: 1rem; align-items: center;">`
- Line 227: `<img src="badge.jpg" ... style="width: 80px; height: auto; border-radius: 0.5rem; flex-shrink: 0;">`
- Line 229: `<p style="font-size: 0.9rem; color: var(--text-muted);">`
- Line 230: `<strong style="color: var(--text);">`
- Line 233: `<p style="font-size: 0.85rem; margin-top: 0.5rem;">`
- Line 234: `<a href="..." style="color: var(--accent); text-decoration: none;">`
- Line 241: `<p style="color: var(--text-muted); font-size: 0.9rem; margin-bottom: 0.75rem;">`
- Lines 264, 270: `<div class="browser-warning" style="display: block;">`

## Impact
- **Maintainability**: Style changes require editing both the stylesheet and multiple inline locations
- **Consistency**: Similar elements (like `<p>` tags) have different styles depending on whether they use inline or class-based styling
- **Reusability**: Inline styles cannot be reused across elements without duplication
- **Cascade conflicts**: Inline styles have highest specificity, making them hard to override with media queries or theme variations

## Evidence
```html
<!-- Line 226-234: Mixed inline and class-based styling -->
<div class="card" style="display: flex; gap: 1rem; align-items: center;">
  <img src="badge.jpg" alt="CDC Badge v1.0" style="width: 80px; height: auto; border-radius: 0.5rem; flex-shrink: 0;">
  <div>
    <p style="font-size: 0.9rem; color: var(--text-muted);">
```

Compare to the stylesheet which defines `.card` but not these specific inline properties:
```css
/* Lines 75-81: .card definition */
.card {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 0.75rem;
  padding: 1.5rem;
  margin-bottom: 1.25rem;
}
```

## Recommended Fix
1. Create dedicated CSS classes for the inline styles:
   ```css
   .card--horizontal {
     display: flex;
     gap: 1rem;
     align-items: center;
   }

   .badge-image {
     width: 80px;
     height: auto;
     border-radius: 0.5rem;
     flex-shrink: 0;
   }

   .text-small {
     font-size: 0.9rem;
     color: var(--text-muted);
   }

   .text-smaller {
     font-size: 0.85rem;
   }

   .mt-1 {
     margin-top: 0.5rem;
   }

   .mb-1 {
     margin-bottom: 0.75rem;
   }

   .link-accent {
     color: var(--accent);
     text-decoration: none;
   }
   ```

2. Replace inline styles with classes:
   ```html
   <div class="card card--horizontal">
     <img src="badge.jpg" alt="CDC Badge v1.0" class="badge-image">
     <div>
       <p class="text-small">
   ```

3. For the `display: block` on `.browser-warning`, ensure the base CSS sets `display: none` and use a class like `.is-visible` or JavaScript to toggle visibility instead of inline styles.

## References
- [MDN: Inline styles](https://developer.mozilla.org/en-US/docs/Web/CSS/Specificity#inline_styles)
- [CSS Specificity hierarchy](https://developer.mozilla.org/en-US/docs/Web/CSS/Specificity)
