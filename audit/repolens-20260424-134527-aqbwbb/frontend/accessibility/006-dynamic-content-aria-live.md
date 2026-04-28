---
title: "[LOW] Dynamic content updates lack aria-live region"
severity: LOW
domain: frontend
lens: a11y
labels:
  - "audit:frontend/accessibility"
---

## Summary
The version badge that displays the latest GitHub release updates dynamically via JavaScript, but lacks an `aria-live` region. Screen reader users will not be notified when the version information changes from "Checking latest version..." to the actual version number.

**Location:** `web-flasher/index.html`, lines 65-72 and 289-307

```html
<!-- Line 65-72 -->
<span class="version-badge loading" id="version-badge">Checking latest version...</span>

<!-- Lines 289-307 -->
<script>
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
fetchLatestVersion();
</script>
```

## Impact
- **Screen reader users miss dynamic updates** - The version change is not announced
- **Uncertainty about page state** - Users don't know if loading completed successfully or failed
- **WCAG 2.1 Success Criterion 4.1.3 (Status Messages)** - Not fully met
- Users relying on screen readers may not realize the page has finished loading version info

## Evidence
- Line 67: Initial state "Checking latest version..."
- Line 299-300: JavaScript updates `badge.textContent` without notifying assistive technology
- Line 303: Error state "Version info unavailable" also not announced
- No `aria-live`, `aria-atomic`, or `role="status"` attributes present

## Recommended Fix
Add `aria-live` region to the version badge:

```html
<span 
  class="version-badge loading" 
  id="version-badge" 
  role="status" 
  aria-live="polite"
  aria-atomic="true"
>Checking latest version...</span>
```

Or use a wrapper element:

```html
<div aria-live="polite" aria-atomic="true">
  <span class="version-badge loading" id="version-badge">Checking latest version...</span>
</div>
```

For more control, you could also add `aria-busy` during loading:

```html
<script>
async function fetchLatestVersion() {
  const badge = document.getElementById("version-badge");
  badge.setAttribute("aria-busy", "true");
  
  try {
    const resp = await fetch(
      "https://api.github.com/repos/krim404/cdc-badge-os/releases/latest"
    );
    if (!resp.ok) throw new Error("API request failed");
    const data = await resp.json();
    badge.textContent = data.tag_name;
    badge.classList.remove("loading");
    badge.setAttribute("aria-busy", "false");
  } catch {
    badge.textContent = "Version info unavailable";
    badge.setAttribute("aria-busy", "false");
  }
}
fetchLatestVersion();
</script>
```

## References
- [WCAG 2.1 Success Criterion 4.1.3 Status Messages](https://www.w3.org/TR/WCAG21/#status-messages)
- [MDN: aria-live](https://developer.mozilla.org/en-US/docs/Web/Accessibility/ARIA/Attributes/aria-live)
- [WAI-ARIA Authoring Practices - Status](https://www.w3.org/WAI/ARIA/apg/patterns/status/)
