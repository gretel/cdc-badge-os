---
title: "[LOW] Unoptimized image format for badge.jpg"
severity: LOW
domain: frontend-perf
lens: frontend-performance
labels:
  - "image-optimization"
  - "web-flasher"
---

## Summary
The web-flasher uses `badge.jpg` (7.5KB) for displaying the CDC Badge image. JPG is not the optimal format for this type of image which likely contains text, sharp edges, and solid colors.

**Location:** `web-flasher/index.html:222`
**File:** `web-flasher/badge.jpg` (7.5KB)

## Impact
- JPG format is inefficient for images with sharp edges, text, or solid colors
- The image could likely be 30-50% smaller with PNG or WebP format
- Total page weight is small (~17KB), but every byte matters for fast loading
- No alt text optimization for the image (minor accessibility impact)

## Evidence
```html
<!-- Line 222 -->
<img src="badge.jpg" alt="CDC Badge v1.0" style="width: 80px; height: auto; border-radius: 0.5rem; flex-shrink: 0;">
```

File size: `badge.jpg` = 7.5KB

The image is displayed at 80px width, suggesting the source may be larger than needed.

## Recommended Fix
1. Convert `badge.jpg` to WebP format for better compression
2. Optimize the image dimensions to match display size (80px width)
3. Consider using PNG if the image has transparency or sharp edges
4. Add a responsive `srcset` if larger displays benefit from higher resolution

Example conversion:
```bash
# Using cwebp for WebP conversion
cwebp -q 80 -w 160 badge.jpg -o badge.webp

# Or using sharp (Node.js)
npx sharp badge.jpg -q 80 -w 160 badge.webp
```

Update HTML:
```html
<picture>
  <source srcset="badge.webp" type="image/webp">
  <img src="badge.jpg" alt="CDC Badge v1.0" style="width: 80px; height: auto; border-radius: 0.5rem; flex-shrink: 0;">
</picture>
```

## References
- [Web.dev: Optimize images](https://web.dev/optimize-images/)
- [MDN: Responsive images](https://developer.mozilla.org/en-US/docs/Learn/HTML/Multimedia_and_embedding/Responsive_images)
- [WebP format](https://developers.google.com/speed/webp)
