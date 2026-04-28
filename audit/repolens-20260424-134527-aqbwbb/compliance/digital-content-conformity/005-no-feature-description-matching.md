---
title: "[LOW] Feature descriptions may not match actual implementation (WIP features)"
severity: LOW
domain: digital-content-conformity
lens: EU-2019-770
labels:
  - "feature-description"
---

## Summary
The README and documentation list several features as "WIP" (Work In Progress), but there is no clear distinction between features that are usable vs. features that are incomplete. Under EU Directive 2019/770, digital content must match its description - unclear status may mislead users about feature availability.

**Files affected:**
- `README.md` - Feature status table (lines 13-29)
- `docs/README.md` - Module status table (lines 28-35)

## Evidence
1. **Ambiguous "WIP" status:**
   - `README.md:20` - "GPG/CCID | WIP | OpenPGP smartcard via USB CCID"
   - `README.md:21-22` - "BLE vCard | WIP", "BLE HID | WIP"
   - `README.md:24` - "BLE Serial | WIP"
   - No definition of what "WIP" means (usable? beta? incomplete?)

2. **Contradictory status:**
   - `docs/README.md:33` - "BLE vCard | Working"
   - `README.md:21` - "BLE vCard | WIP"
   - Inconsistent information between docs

3. **No feature flag documentation for users:**
   - `components/cdc_core/feature_flags.h` - Feature flags exist but not documented for end users
   - Users cannot know if a feature is enabled on their device

## Recommended Fix
1. **Standardize feature status terminology:**
   - Define clear status levels: "Working", "Beta", "Alpha", "Planned"
   - Update both README.md and docs/README.md consistently

2. **Add feature availability notes:**
   - For WIP features, add notes like "Limited functionality" or "Experimental"
   - Link to known issues or limitations

3. **Document feature flags for users:**
   - Add section in README explaining how to check enabled features
   - Add serial command `FEATURES` to list enabled modules

## References
- EU Directive 2019/770 Article 6 (Contractual conformity - features must match description)
- EU Directive 2019/770 Article 7 (Information requirements)
