---
title: "[MEDIUM] Web flasher loads JavaScript from US-based unpkg.com CDN"
severity: MEDIUM
domain: digital-sovereignty
lens: cdn-dependency
labels:
  - "audit:compliance/sovereignty"
---

## Summary

The web flasher (`web-flasher/index.html:9-10`) loads the `esp-web-tools` library from `unpkg.com`, a US-based CDN (Unpkg, owned by New York-based company):

```html
<script
  type="module"
  src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
></script>
```

This creates a dependency on US infrastructure for the firmware flashing experience.

## Impact

- **Moderate risk**: The web flasher is the primary user-facing tool for firmware installation
- Unpkg.com serves US-centric content and could be affected by US jurisdiction
- If unpkg.com becomes unavailable or blocks EU access, users cannot flash firmware via browser
- The CDN could theoretically serve modified JavaScript (though esp-web-tools is open source)
- Affects user experience but not core device functionality

## Evidence

**File**: `web-flasher/index.html`  
**Line**: 9-10

```html
<script
  type="module"
  src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
></script>
```

## Recommended Fix

**Option A: Self-host the JavaScript library**

Download `esp-web-tools` and host it locally in the web-flasher directory:

```bash
# Download the library
curl -o web-flasher/esp-web-tools.js https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module
```

Then update the HTML:
```html
<script type="module" src="esp-web-tools.js"></script>
```

**Option B: Use a European CDN**

Replace with a EU-based CDN like:
- [jsDelivr](https://www.jsdelivr.com/) (has EU edge servers)
- [Bunny.net](https://bunny.net/) (US company but EU data centers)
- [KeyCDN](https://www.keycdn.com/) (Germany-based)

**Option C: Bundle with GitHub Pages deployment**

Download the library during CI/CD and include it in the build artifact.

## References

- [unpkg.com - US CDN](https://unpkg.com/)
- [jsDelivr - Global CDN with EU presence](https://www.jsdelivr.com/)
- [esp-web-tools GitHub](https://github.com/estefaniap/esp-web-tools)

---
