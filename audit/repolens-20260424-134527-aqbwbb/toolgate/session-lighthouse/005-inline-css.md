---
title: "[MEDIUM] Inline CSS could be extracted or minified"
severity: MEDIUM
domain: cdc-badge-os
lens: session-lighthouse
labels:
  - "audit:toolgate/session-lighthouse"
---

## Summary
The web-flasher uses a large inline `<style>` block (lines 11-216, ~205 lines). While this avoids render-blocking for a single-page app, the CSS could be minified to reduce file size.

**File:** `web-flasher/index.html` (lines 11-216)

## Impact
- **File size**: ~2KB of CSS that could be minified to ~1.5KB
- **Lighthouse score**: May flag "unminified-css" opportunity
- **Maintainability**: Inline styles make it harder to cache and reuse

## Evidence
Current implementation (excerpt):
```html
<style>
  :root {
    :root {
      --bg: #0d1117;
      --surface: #161b22;
      --border: #30363d;
      --text: #e6edf3;
      --text-muted: 8b949e;
      --accent: #58a6ff;
      --accent-hover: #79c0ff;
      --warning: #d29922;
      --step-bg: #1c2128;
    }
  }

  * { margin: 0; padding: 0; box-sizing: border-box; }

  body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
    background: var(--bg);
    color: var(--text);
    line-height: 1.6;
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    align-items: center;
  }
  /* ... 180+ more lines ... */
</style>
```

## Recommended Fix
**Option 1: Minify inline CSS** (simplest for single-page):
Use a CSS minifier to compress the styles:
```bash
# Using cssnano CLI
npx cssnano index.html output.html
```

**Option 2: External stylesheet** (better for caching):
1. Create `web-flasher/styles.css`
2. Move CSS to external file
3. Link in HTML:
```html
<link rel="stylesheet" href="styles.css">
```

**Option 3: Critical CSS inlined, rest external** (best performance):
- Inline above-the-fold styles
- Load remaining CSS asynchronously

For this single-page flasher, **Option 1** (minify inline) is recommended since:
- Only one page to optimize
- Avoids extra HTTP request
- Simpler deployment

## References
- [Lighthouse: Unminified CSS](https://web.dev/unminified-css/)
- [CSS Minification tools](https://developers.google.com/speed/docs/insights/MinifyCSS)
