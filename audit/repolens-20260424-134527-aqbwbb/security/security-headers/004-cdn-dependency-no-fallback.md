---
title: "[LOW] CDN Dependency Without Fallback"
severity: LOW
domain: security-headers
lens: security-headers
labels:
  - "audit:security/security-headers"
---

## Summary
The web-flasher at `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html` depends entirely on unpkg.com CDN for the esp-web-tools library with no fallback for when the CDN is unavailable.

## Impact
- If unpkg.com is down or slow, the web flasher won't work
- Users may have difficulty flashing firmware if the CDN has connectivity issues
- No graceful degradation for older browsers that don't support Web Serial API

## Evidence
File: `web-flasher/index.html`
- Line 7-10: Script loaded from external CDN:
  ```html
  <script
    type="module"
    src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"
  ></script>
  ```
- Line 266-268: No fallback content for when the script fails to load

## Recommended Fix
Add a fallback mechanism:

1. **Add a simple fallback message** (after line 268):
   ```html
   <noscript>
     <div class="card">
       <h2>JavaScript Required</h2>
       <p>Please enable JavaScript to use the web flasher.</p>
     </div>
   </noscript>
   ```

2. **Add script error handling** (in the `<script>` section at line 300):
   ```html
   <script>
     // Check if esp-web-tools loaded
     document.addEventListener('DOMContentLoaded', () => {
       const button = document.querySelector('esp-web-install-button');
       if (!button) {
         console.warn('esp-web-tools failed to load');
         // Show fallback UI here
       }
     });
   </script>
   ```

3. **Consider hosting the library locally** to reduce external dependencies.

## References
- [Web Serial API Support](https://caniuse.com/web-serial)
- [Progressive Enhancement Best Practices](https://developer.mozilla.org/en-US/docs/Glossary/Progressive_Enhancement)
