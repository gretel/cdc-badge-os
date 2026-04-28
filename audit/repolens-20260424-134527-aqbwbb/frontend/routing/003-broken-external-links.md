---
title: "[LOW] Potentially Broken External Link to CDC Badge Repository"
severity: LOW
domain: frontend
lens: routing
labels:
  - "audit:frontend/routing"
---

## Summary
The web-flasher contains an external link to `https://github.com/krim404/cdc-badge-os` which may not be the canonical repository. The README and other files reference `https://github.com/riatlabs/cdc-badge` for hardware details.

**Location:** `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html:288`

```html
<a href="https://github.com/krim404/cdc-badge-os">CDC Badge OS on GitHub</a>
```

## Impact
- **User confusion**: Different GitHub organization/user references may confuse users
- **Potential broken link**: If the `krim404` repository is a fork or mirror, users may not find the latest version
- **Inconsistent documentation**: Hardware link points to `riatlabs`, firmware link points to `krim404`

## Evidence
1. `web-flasher/index.html:288` links to `https://github.com/krim404/cdc-badge-os`
2. `web-flasher/index.html:246` links to `https://github.com/riatlabs/cdc-badge` for hardware
3. The Doxyfile references components from the current repository structure

## Recommended Fix
Verify which repository is the canonical source and update the link accordingly:

1. Check if `https://github.com/krim404/cdc-badge-os` is the main repository or a fork
2. Update the link to point to the canonical repository
3. Consider using a consistent GitHub organization across all links

Example fix:
```html
<a href="https://github.com/krim404/cdc-badge-os">CDC Badge OS on GitHub</a>
```

Should be changed to match the canonical repository (likely `riatlabs/cdc-badge-os` or similar).

## References
- Web-flasher index.html
- README.md repository references
