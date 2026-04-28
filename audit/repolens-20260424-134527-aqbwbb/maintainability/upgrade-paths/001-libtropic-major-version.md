---
title: "[MEDIUM] libtropic v3.0.0 to v3.2.1 upgrade available"
severity: MEDIUM
domain: dependencies
lens: upgrade-paths
labels:
  - "audit:maintainability/upgrade-paths"
---

## Summary
The `third_party/libtropic` submodule is pinned to version **v3.0.0** (commit `e730ebfb`), but version **v3.2.1** is available. This represents 2 minor versions and 3 patch releases behind the latest stable release.

**Evidence:**
- Current version: `third_party/libtropic (v3.0.0)` - commit `e730ebfb585483be2347e8a96d1722806ca8d2ca`
- Latest version: **v3.2.1** (released 2026-04-21)
- File: `.gitmodules` and git submodule status

## Impact
**Benefits of upgrading:**
- **v3.2.1**: Fixed FW update reboot requirement bug
- **v3.2.0**: Size fix for `l3_chunk` member, improved `lt_init()` behavior for Start-up Mode, STM32 secure random generation
- **v3.1.0**: Restructured documentation, new tutorials, improved logging (LF handling)

**Risk:**
- Minor API changes in v3.1.0 (merged platform repositories, restructured examples)
- Custom patches may need review (project applies `libtropic_espidf_gcm_workaround.patch`)

## Evidence
From `git submodule status`:
```
e730ebfb585483be2347e8a96d1722806ca8d2ca third_party/libtropic (v3.0.0)
```

Current submodule in `.gitmodules`:
```
[submodule "third_party/libtropic"]
    path = third_party/libtropic
    url = https://github.com/tropicsquare/libtropic.git
```

Latest release v3.2.1 changelog highlights:
- FW update: added necessary reboot into Maintenance Mode before updating second FW bank pair

## Recommended Fix
1. **Review compatibility**: Check if your TROPIC01 hardware firmware versions (Application FW, SPECT FW, Bootloader FW) are compatible with libtropic v3.2.1. According to the compatibility table, v3.2.1 supports Application FW 1.0.0–2.0.0 with Bootloader FW 2.0.1.

2. **Update submodule**:
   ```bash
   cd third_party/libtropic
   git fetch --tags
   git checkout v3.2.1
   cd ../..
   git add third_party/libtropic
   git commit -m "chore: update libtropic from v3.0.0 to v3.2.1"
   ```

3. **Test existing patches**: Verify that `patches/libtropic_espidf_gcm_workaround.patch` still applies cleanly to v3.2.1. The patch modifies `cal/mbedtls_v4/lt_mbedtls_v4_aesgcm.c` which may have changed.

4. **Build and test**: Run full build and test FIDO2, password vault, and GPG modules that use libtropic.

## References
- [libtropic releases](https://github.com/tropicsquare/libtropic/releases)
- [libtropic compatibility table](https://github.com/tropicsquare/libtropic/blob/master/README.md)
- [libtropic changelog](https://github.com/tropicsquare/libtropic/blob/master/CHANGELOG.md)
