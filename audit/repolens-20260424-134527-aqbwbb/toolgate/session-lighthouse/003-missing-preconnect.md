---
title: "[MEDIUM] Missing preconnect for external CDN"
severity: MEDIUM
domain: cdc-badge-os
lens: session-lighthouse
labels:
  - "audit:toolgate/session-lighthouse"
---

## Summary
The page loads JavaScript from `unpkg.com` CDN without a preconnect link, adding unnecessary latency to the critical rendering path.

**File:** `web-flasher/index.html` (line 7-10)

## Impact
- **Performance impact**: Extra round-trip time (RTT) to establish connection to unpkg.com
- **Lighthouse score**: Will flag "uses-rel-preconnect" opportunity
- **User experience**: Slightly delayed loading of esp-web-tools module

## Evidence
Current implementation (lines 7-10):
```html
<script
  type="module"
  src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
></script>
```

No preconnect link is present in the `<head>`.

## Recommended Fix
Add a `<link rel="preconnect">` tag before the script to establish early connection:

```html
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>CDC Badge OS - Web Flasher</title>
  <link rel="preconnect" href="https://unpkg.com">
  <link rel="modulepreload" href="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js">
  <script
    type="module"
    src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
  ></script>
```

**Options:**
1. **preconnect** - Establishes connection early (DNS, TCP, TLS)
2. **modulepreload** - Preloads the module (more aggressive, better for critical resources)

Use `modulepreload` if you want to aggressively optimize, or `preconnect` for a lighter approach.

## References
- [Lighthouse: Uses-rel-preconnect](https://web.dev/uses-rel-preconnect/)
- [MDN link rel="preconnect"](https://developer.mozilla.org/en-US/docs/Web/HTML/Link_types/preconnect)
- [MDN link rel="modulepreload"](https://developer.mozilla.org/en-US/docs/Web/HTML/Attributes/rel/modulepreload)
