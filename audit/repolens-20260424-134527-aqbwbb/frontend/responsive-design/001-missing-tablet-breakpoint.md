---
title: "[MEDIUM] Missing tablet breakpoint in web flasher"
severity: MEDIUM
domain: frontend
lens: responsive-design
labels:
  - responsive-design
  - mobile-first
  - css
---

## Summary
The CDC Badge OS web flasher (`web-flasher/index.html`) has a mobile breakpoint at 480px but lacks a dedicated tablet breakpoint (768px-1024px range). The current CSS only has a single media query for mobile devices at 480px width.

**Location**: `web-flasher/index.html:212-215`

```css
@media (max-width: 480px) {
  .container { padding: 1.25rem 1rem; }
  header h1 { font-size: 1.4rem; }
}
```

The layout uses `max-width: 640px` on the container (`web-flasher/index.html:38`), which works well for mobile but doesn't optimize for tablet-sized screens (768px-1024px).

## Impact
**Moderate usability impact** on tablet devices:
- Tablet users (iPad, Android tablets) get the same desktop layout without any optimizations
- The 640px max-width container leaves significant whitespace on tablet screens
- No adaptive adjustments for tablet portrait vs. landscape orientations
- Inconsistent experience across the device spectrum (mobile → tablet → desktop)

## Evidence
Current breakpoint coverage:
- Mobile: `< 480px` (has media query at line 212)
- Tablet: `768px - 1024px` (NO media query)
- Desktop: `> 1024px` (base styles)

The single breakpoint at 480px creates a large gap where tablet devices receive no specialized styling.

## Recommended Fix
Add a tablet breakpoint to optimize the layout for 768px-1024px screens:

```css
@media (min-width: 481px) and (max-width: 1024px) {
  .container {
    padding: 1.5rem 2rem;
  }
  
  header h1 {
    font-size: 1.6rem;
  }
  
  /* Optional: Adjust card layouts for tablet */
  .card {
    padding: 1.25rem;
  }
}
```

Or use a simpler mobile-first approach:
```css
@media (max-width: 1024px) {
  .container {
    padding: 1.5rem;
  }
}
```

This should be added before the existing 480px breakpoint to maintain proper cascade order.

## References
- [MDN - CSS Media Queries](https://developer.mozilla.org/en-US/docs/Web/CSS/Media_Queries/Using_media_queries)
- [Google - Essential Responsive Design Patterns](https://web.dev/responsive-web-design-basics/)
- Common breakpoints: Mobile (<768px), Tablet (768-1024px), Desktop (>1024px)
