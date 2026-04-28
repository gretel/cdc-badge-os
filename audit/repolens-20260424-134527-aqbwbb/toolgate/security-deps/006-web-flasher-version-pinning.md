---
title: "[LOW] Web flasher uses CDN-hosted esp-web-tools without version pinning"
severity: LOW
domain: security-deps
lens: toolgate/security-deps
labels:
  - "audit:toolgate/security-deps"
---

## Summary
The web flasher (`web-flasher/index.html`) loads `esp-web-tools` from unpkg CDN using a major version range (`@10`) instead of a specific version. This can lead to unexpected behavior when new versions are released.

**File Reference:** `web-flasher/index.html` line 8

**Affected Dependency:**
- Package: `esp-web-tools`
- Current version loaded: `@10` (resolves to latest 10.x)
- Location: Web flasher for ESP32 firmware updates

## Impact
- **Maintenance Risk:** New major/minor versions may introduce breaking changes
- **Consistency:** Different users may get different versions based on CDN caching
- **Reproducibility:** Harder to reproduce exact behavior for debugging
- **Security:** CDN-hosted libraries can be compromised (though unpkg is generally reliable)

While this is not a critical vulnerability (esp-web-tools v10 is current and well-maintained), it's a best practice to pin versions for production deployments.

## Evidence
**Current implementation (`web-flasher/index.html`):**
```html
<script
  type="module"
  src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
></script>
```

**Latest available version:** 10.2.1 (as of 2026-01-20)

**Version range behavior:**
- `@10` resolves to latest 10.x.x (currently 10.2.1)
- Could silently upgrade to 10.3.0, 10.4.0, etc.
- Breaking changes in minor versions may affect functionality

## Recommended Fix
**Pin esp-web-tools to a specific version**

1. **Update to specific version**:
   ```html
   <script
     type="module"
     src="https://unpkg.com/esp-web-tools@10.2.1/dist/web/install-button.js?module"
   ></script>
   ```

2. **Optional: Add integrity check** (Subresource Integrity):
   ```html
   <script
     type="module"
     src="https://unpkg.com/esp-web-tools@10.2.1/dist/web/install-button.js?module"
     integrity="sha384-..."
     crossorigin="anonymous"
   ></script>
   ```

3. **Alternative: Bundle locally** (for better control):
   ```bash
   # Install locally
   npm install esp-web-tools@10.2.1
   
   # Bundle with web flasher
   # Update import path in index.html
   ```

4. **Add version check script**:
   ```javascript
   // Add version check for debugging
   fetch('https://unpkg.com/esp-web-tools@10.2.1/package.json')
     .then(r => r.json())
     .then(data => console.log('esp-web-tools version:', data.version));
   ```

## References
- esp-web-tools: https://github.com/esphome/esp-web-tools
- unpkg: https://unpkg.com/
- SRI Hash Generator: https://www.srihash.org/
- Web Flasher: `web-flasher/index.html`
