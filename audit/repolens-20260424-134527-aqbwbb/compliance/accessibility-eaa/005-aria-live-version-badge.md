---
title: "[LOW] Missing aria-label for version badge dynamic content"
severity: LOW
domain: web-flasher
lens: accessibility-eaa
labels:
  - "wcag-2.1"
  - "aria"
  - "dynamic-content"
---

## Summary
The version badge element at `web-flasher/index.html` (line 224) displays dynamic content that updates via JavaScript, but does not use ARIA attributes to communicate state changes to screen readers.

## Impact
Screen reader users may not be aware when the version information updates from "Checking latest version..." to the actual version number. The element uses `aria-live` implicitly but could benefit from explicit ARIA attributes for better clarity.

## Evidence
File: `web-flasher/index.html` (lines 224, 290-305):
```html
<span class="version-badge loading" id="version-badge">Checking latest version...</span>
```

```javascript
async function fetchLatestVersion() {
  const badge = document.getElementById("version-badge");
  try {
    const resp = await fetch(
      "https://api.github.com/repos/krim404/cdc-badge-os/releases/latest"
    );
    if (!resp.ok) throw new Error("API request failed");
    const data = await resp.json();
    badge.textContent = data.tag_name;
    badge.classList.remove("loading");
  } catch {
    badge.textContent = "Version info unavailable";
  }
}
```

## Recommended Fix
Add ARIA attributes to communicate the dynamic content:

```html
<span class="version-badge loading" id="version-badge" aria-live="polite" aria-atomic="true">
  Checking latest version...
</span>
```

Optionally add a label for clarity:
```html
<span class="version-badge loading" id="version-badge" aria-live="polite" aria-atomic="true" role="status">
  <span class="visually-hidden">Latest version:</span>
  Checking latest version...
</span>
```

## References
- [WCAG 2.1 SC 4.1.3 Status Messages](https://www.w3.org/WAI/WCAG21/Understanding/status-messages.html)
- [ARIA Live Regions](https://developer.mozilla.org/en-US/docs/Web/Accessibility/ARIA/ARIA_Live_Regions)
