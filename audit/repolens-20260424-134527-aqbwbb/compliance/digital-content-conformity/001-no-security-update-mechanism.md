---
title: "[MEDIUM] No mechanism to provide security updates for firmware post-delivery"
severity: MEDIUM
domain: digital-content-conformity
lens: EU-2019-770
labels:
  - "update-obligation"
---

## Summary
The CDC Badge OS firmware is delivered as downloadable binaries via GitHub releases, but there is no built-in mechanism to notify users of security updates or provide a structured update path after initial download. The firmware has no in-device version check or update notification system.

**Files affected:**
- `tools/flash_firmware.py` - Flash tool with release download capability
- `web-flasher/index.html` - Web-based firmware flasher
- `.github/workflows/build.yml` - Release workflow

## Impact
Under EU Directive 2019/770 Article 11, consumers have the right to receive updates (including security updates) for digital content for a "reasonable period" which should be documented. Without a documented update mechanism:
- Users may not know when security patches are available
- No clear definition of how long updates will be provided
- Potential security vulnerabilities remain unpatched on deployed devices

## Evidence
1. **Flash tool downloads releases but no update check:**
   - `tools/flash_firmware.py:98-110` - Downloads latest release but no background update check
   - No mechanism to compare current device version with latest release

2. **No in-device update mechanism:**
   - No code in `components/` directory for version checking or OTA updates
   - No notification system for available updates

3. **Release workflow auto-generates notes but no structured changelog:**
   - `.github/workflows/build.yml:88` - `generate_release_notes: true` (auto-generated, not curated)
   - No `CHANGELOG.md` or structured release notes in repository root

## Recommended Fix
Implement a basic update notification system:

1. **Add version tracking to firmware:**
   - Define `FIRMWARE_VERSION` in a central header (e.g., `components/cdc_core/include/cdc_core/version.h`)
   - Store version in NVS for retrieval

2. **Create update check command:**
   - Add serial command `UPDATE_CHECK` that queries GitHub API for latest release
   - Compare current version with latest release
   - Display available update info on display

3. **Document update policy:**
   - Add `UPDATES.md` to docs explaining:
     - How long updates will be provided (e.g., "2 years from release")
     - How users can check for updates
     - What types of updates to expect (security, feature, bug fixes)

4. **Optional - Create CHANGELOG.md:**
   - Structure release notes with categories (Security, Features, Bug Fixes)
   - Link to relevant commits/PRs

## References
- EU Directive 2019/770 Article 11 (Updates)
- EU Directive 2019/770 Article 13 (Termination of contracts for digital content)
- Best practices for embedded firmware update mechanisms (ESP-IDF OTA update documentation)
