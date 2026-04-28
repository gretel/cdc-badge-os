---
title: "[MEDIUM] Missing meta description for SEO"
severity: MEDIUM
domain: cdc-badge-os
lens: session-lighthouse
labels:
  - "audit:toolgate/session-lighthouse"
---

## Summary
The `web-flasher/index.html` page is missing a meta description tag, which is a basic SEO requirement that Lighthouse checks.

**File:** `web-flasher/index.html` (lines 3-10, in `<head>`)

## Impact
- **SEO impact**: Search engines may not display a meaningful snippet in search results
- **Lighthouse score**: Will fail the "document has meta description" audit
- **Social sharing**: Meta descriptions are used for Open Graph previews

## Evidence
Current `<head>` section (lines 3-10):
```html
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, add-scale=1.0">
  <title>CDC Badge OS - Web Flasher</title>
  <script
    type="module"
    src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
  ></script>
  <style>
```

Missing:
```html
<meta name="description" content="...">
```

## Recommended Fix
Add a meta description tag in the `<head>` section:

```html
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>CDC Badge OS - Web Flasher</title>
  <meta name="description" content="Official web-based firmware flasher for CDC Badge v1.0. Flash ESP32-S3 firmware to your hardware security key using Web Serial API.">
  <script
    type="module"
    src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
  ></script>
```

Description should be 150-160 characters and accurately describe the page content.

## References
- [Lighthouse SEO: meta-description](https://web.dev/meta-description/)
- [MDN Meta Description](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/meta#description)
