---
title: "[LOW] Missing preload hint for esp-web-tools module"
severity: LOW
domain: frontend-perf
lens: frontend-performance
labels:
  - "resource-hints"
  - "web-flasher"
---

## Summary
The esp-web-tools module is loaded from CDN (`https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module`) without preloading. Adding a preload hint can improve the time to interactive for the flasher button.

**Location:** `web-flasher/index.html:7-10`

## Impact
- The module is render-blocking (loaded in `<head>`)
- Without preload, the browser discovers the module after parsing the script tag
- Preloading can save ~100-200ms on slower connections
- Critical for the main functionality (the flasher button)

## Evidence
```html
<!-- Lines 7-10 -->
<script
  type="module"
  src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
></script>
```

The script is loaded synchronously in the head without preload hints.

## Recommended Fix
Add a preload link in the head section:

```html
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>CDC Badge OS - Web Flasher</title>
  <link rel="modulepreload" href="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js">
  <script
    type="module"
    src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
  ></script>
  ...
</head>
```

Or use `<link rel="preload">` with `as="script"` and `type="module"`:

```html
<link rel="preload" href="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js" as="script" type="module">
```

## References
- [MDN: modulepreload](https://developer.mozilla.org/en-US/docs/Web/HTML/Link_types/modulepreload)
- [Web.dev: modulepreload](https://web.dev/modulepreload/)
