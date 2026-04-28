---
title: "[LOW] Doxygen documentation imports Google Fonts (US-controlled typography)"
severity: LOW
domain: digital-sovereignty
lens: font-dependency
labels:
  - "audit:compliance/sovereignty"
---

## Summary

The Doxygen documentation theme imports the "Inter" font from Google Fonts CDN:

**File**: `third_party/libtropic/docs/doxygen/html/doxygen-awesome.css`
**Line**: Contains `@import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap');`

The CSS file is part of the "Doxygen Awesome" theme used for API documentation generation. The font is loaded from Google's servers (US-based) every time the documentation is viewed in a browser.

## Impact

- **Minor data leakage**: Browser requests to `fonts.googleapis.com` reveal visitor IP addresses to Google
- **US jurisdiction**: Google Fonts are served from US-controlled infrastructure
- **Not critical**: Documentation is static, no sensitive data transmitted
- **No CLOUD Act exposure for user data** (only visitor metadata)
- **Build-time dependency**: Doxygen download already has US fallback (SourceForge)

For a sovereignty-focused project documenting a hardware security key, even minor external dependencies should be evaluated.

## Evidence

**File**: `third_party/libtropic/docs/doxygen/html/doxygen-awesome.css`

```css
@import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap');
```

**Context**: This is part of the Doxygen documentation generated for the libtropic SDK (third-party secure element library).

**Provider**: Google Fonts (Google LLC, USA)

## Recommended Fix

**Option A: Self-host Google Fonts**

1. Download the Inter font files:
   ```bash
   mkdir -p docs/fonts
   # Download Inter font from https://fonts.google.com/specimen/Inter
   ```

2. Update CSS to use local fonts:
   ```css
   @font-face {
     font-family: 'Inter';
     src: url('../fonts/Inter-Regular.woff2') format('woff2');
     font-weight: 400;
   }
   @font-face {
     font-family: 'Inter';
     src: url('../fonts/Inter-Medium.woff2') format('woff2');
     font-weight: 500;
   }
   @font-face {
     font-family: 'Inter';
     src: url('../fonts/Inter-SemiBold.woff2') format('woff2');
     font-weight: 600;
   }
   @font-face {
     font-family: 'Inter';
     src: url('../fonts/Inter-Bold.woff2') format('woff2');
     font-weight: 700;
   }
   ```

**Option B: Use system fonts**

Replace the Google Fonts import with a system font stack (no external dependency):

```css
font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
```

**Option C: Use European font hosting**

- [Fontshare](https://www.fontshare.com/) (Indian, but alternative)
- Self-hosted font CDN (e.g., on Hetzner/OVH)

## References

- [Google Fonts](https://fonts.google.com/)
- [Inter Font Family](https://rsms.me/inter/) - Open source, can self-host
- [Doxygen Awesome Theme](https://github.com/jothepro/doxygen-awesome-css)
- [European Font Self-Hosting](https://github.com/rsms/inter) - Inter is open source and MIT licensed

---
