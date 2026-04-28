---
title: "[MEDIUM] No in-device firmware update mechanism"
severity: MEDIUM
domain: compliance/product-liability
lens: product-liability
labels:
  - update-mechanism
  - user-experience
---

## Summary
The firmware has no built-in mechanism to check for or apply updates. Users must manually flash firmware using the web flasher or Python tool. No auto-update or update-check feature exists.

**Evidence:**
- No code searching for "auto.*update\|check.*update\|version.*check" in firmware
- Flash tool (`flash_firmware.py`) requires manual invocation
- Web flasher shows latest version but doesn't auto-update

## Impact
- Users may not know when new versions (including security patches) are available
- Manual flashing process has friction, reducing likelihood of updates
- Under Product Liability Directive, software should have reasonable update mechanisms
- Critical security patches may not reach users in a timely manner

## Evidence
Search for update mechanisms:
```bash
grep -rn "auto.*update\|check.*update\|version.*check\|update.*available" /input/20260423-132359-oj8ayc/cdc-badge-os --include="*.cpp" --include="*.h" --include="*.c"
# No relevant results
```

Web flasher (`web-flasher/index.html`) fetches latest version:
```javascript
async function fetchLatestVersion() {
  const resp = await fetch(
    "https://api.github.com/repos/krim404/cdc-badge-os/releases/latest"
  );
  // Shows version but no auto-update logic
}
```

## Recommended Fix
Implement a simple update check mechanism:

1. **Add version check to settings menu**:
   - On device, check GitHub API for latest release
   - Display: "Current: v0.5.0, Latest: v0.6.0 (tap to download)"
   - Download update to device storage (if space allows)

2. **Add serial command for update check**:
   ```
   CHECK_UPDATE -> returns {current: "0.5.0", latest: "0.6.0", url: "..."}
   ```

3. **Document in user guide**: How to check for updates regularly

For firmware, full OTA may be complex. Minimum viable:
- Version check on boot (once per week, cached)
- Visual indicator on UI (e.g., small "N" for "New version available")

## References
- ESP32 OTA Update: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/esp-idf/api-guides/ota.html
- FIDO2 Update Considerations: https://fidoalliance.org/specs/fido-v2.1-rd-20210209/fido-device-onboarding-v2.1-rd-20210209.html
