---
title: "[LOW] No dedicated CHANGELOG.md for user-facing release notes"
severity: LOW
domain: compliance/product-liability
lens: product-liability
labels:
  - release-management
  - documentation
---

## Summary
The project lacks a dedicated `CHANGELOG.md` file. While GitHub releases auto-generate notes, there is no centralized, user-friendly changelog documenting changes, security fixes, and version history.

**Evidence:**
- No `CHANGELOG.md` in repository root
- GitHub Actions uses `generate_release_notes: true` (auto-generated from commits)
- `third_party/libtropic/CHANGELOG.md` exists (for dependency only)

## Impact
- Users cannot quickly see what changed between versions
- Security fixes may be buried in auto-generated commit lists
- Harder to track which versions have specific fixes
- Not following "Keep a Changelog" best practices

## Evidence
```bash
find /input/20260423-132359-oj8ayc/cdc-badge-os -maxdepth 2 -name "CHANGELOG*" 2>/dev/null
# No results (only in third_party dependencies)
```

GitHub workflow (`build.yml`):
```yaml
- name: Create Release
  uses: softprops/action-gh-release@v1
  with:
    files: artifacts/*
    generate_release_notes: true  # Auto-generated, not curated
```

## Recommended Fix
Create `CHANGELOG.md` following [Keep a Changelog](https://keepachangelog.com/) format:

```markdown
# Changelog

## [0.5.0] - 2026-02-15

### Added
- BLE HID keyboard module for auto-type
- Web Flasher with version display

### Changed
- Improved PIN lockout timing

### Security
- Fixed potential timing side-channel in PIN verification

## [0.4.1] - 2026-01-20
...
```

Update GitHub Actions to optionally append curated notes or maintain changelog manually before releases.

## References
- Keep a Changelog: https://keepachangelog.com/
- Semantic Versioning: https://semver.org/
