---
title: "[LOW] No structured version tracking for delivered firmware"
severity: LOW
domain: digital-content-conformity
lens: EU-2019-770
labels:
  - "version-tracking"
---

## Summary
While firmware builds include Git commit hashes and release tags are created, there is no structured version information embedded in the firmware itself that can be queried post-deployment. Users cannot easily determine which version they have running without external tools.

**Files affected:**
- `.github/workflows/build.yml` - Build and release workflow
- `tools/flash_firmware.py` - Flash tool with version detection
- `components/cdc_core/` - Core module (no version header)

## Evidence
1. **Version only in CI artifacts, not firmware:**
   - `.github/workflows/build.yml:38-50` - Version extracted at build time
   - Artifacts named with version: `cdc-badge-firmware-${VERSION}.bin`
   - No `VERSION.h` or similar in source code

2. **Flash tool downloads by release but no device query:**
   - `tools/flash_firmware.py` - Downloads releases by tag
   - No `VERSION` serial command to query current firmware version
   - Users must count commits or check build date to determine version

3. **No structured version header:**
   - Search for `#define VERSION` in `components/` - No matches
   - No semantic versioning (MAJOR.MINOR.PATCH) in code

## Recommended Fix
Add structured version tracking:

1. **Create `components/cdc_core/include/cdc_core/version.h`:**
   ```c
   #define FIRMWARE_VERSION_MAJOR 0
   #define FIRMWARE_VERSION_MINOR 4
   #define FIRMWARE_VERSION_PATCH 1
   #define FIRMWARE_VERSION "0.4.1"
   ```

2. **Add serial command to query version:**
   - Register `VERSION` command that outputs firmware version
   - Include build date and Git commit hash

3. **Display version in UI:**
   - Add to Settings menu
   - Show on boot screen (optional)

4. **Update release workflow:**
   - Include version in release notes
   - Generate CHANGELOG.md from release tags

## References
- Semantic Versioning 2.0.0 (semver.org)
- ESP-IDF versioning conventions
- EU Directive 2019/770 Article 11 (Updates - version information)
