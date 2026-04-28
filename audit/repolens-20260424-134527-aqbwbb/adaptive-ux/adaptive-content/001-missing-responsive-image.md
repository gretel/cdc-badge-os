---
title: "[MEDIUM] Badge image lacks responsive srcset for different viewport sizes"
severity: MEDIUM
domain: web-flasher
lens: adaptive-content
labels:
  - "responsive-images"
  - "mobile-optimization"
---

## Summary
The `badge.jpg` image in `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html` (line 222) uses a single `src` attribute with no `srcset`, `sizes`, or `<picture>` element for responsive art direction. The same image file is served to all devices regardless of viewport width or pixel density.

**Evidence:**
```html
<img src="badge.jpg" alt="CDC Badge v1.0" style="width: 80px; height: auto; border-radius: 0.5rem; flex-shrink: 0;">
```

The image is displayed at 80px width on mobile but could be larger on desktop. The current CSS sets a fixed `width: 80px` which means:
- On mobile (320-480px): 80px display is appropriate
- On desktop (1920px+): Still 80px, but could show a larger preview if desired
- On high-DPI screens: No 2x/3x variant for crisp rendering

## Impact
- **Bandwidth waste**: Mobile users may download a larger image than needed
- **Visual quality**: High-DPI (Retina) displays show a potentially blurry 1x image
- **No art direction**: The image aspect ratio cannot adapt from landscape to portrait for mobile

## Evidence
- File: `web-flasher/index.html`
- Line: 222
- Current markup: Single `<img>` with inline `width: 80px`

## Recommended Fix
1. Determine the original image dimensions (currently unknown, file is ~7.6KB)
2. Create responsive variants:
   - `badge-80.jpg` (80px wide, for mobile)
   - `badge-160.jpg` (160px wide, for desktop)
   - `badge@2x.jpg` (for high-DPI)
3. Update the markup to use `srcset`:
```html
<img
  src="badge-160.jpg"
  srcset="badge-80.jpg 80w, badge-160.jpg 160w"
  sizes="(max-width: 480px) 80px, 160px"
  alt="CDC Badge v1.0"
  style="height: auto; border-radius: 0.5rem; flex-shrink: 0;"
/>
```

Alternatively, if the image is small enough, add `loading="lazy"` and a `sizes` attribute for better DPI handling.

## References
- MDN: [Responsive images](https://developer.mozilla.org/en-US/docs/Learn/HTML/Multimedia_and_embedding/Responsive_images)
- Web.dev: [Use responsive images](https://web.dev/responsive-images/)
