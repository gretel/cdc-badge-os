---
title: "[LOW] No CHANGELOG or Release Notes Strategy"
severity: LOW
domain: deployment-safety
lens: deployment-safety
labels:
  - "audit:devops/deployment-safety"
---

## Summary

The repository has no `CHANGELOG.md` file or documented release notes strategy. While GitHub Actions automatically generates release notes using `generate_release_notes: true`, there is no:

1. **Standardized changelog format**: No convention for documenting changes
2. **Release notes template**: No structure for release announcements
3. **Version history**: No single source of truth for what changed in each release

**Evidence:**
- No `CHANGELOG.md` in repository root
- No `docs/CHANGELOG.md` or `docs/RELEASES.md`
- `build.yml` uses `generate_release_notes: true` (auto-generated, not curated)

## Impact

**User Communication:**
- Users must diff commits to understand what changed
- No curated release notes highlighting important changes
- Breaking changes may not be clearly documented

**Developer Workflow:**
- No convention for documenting changes in PRs
- Release process relies on auto-generated notes
- Hard to track feature progression over time

**Adoption:**
- Potential users cannot quickly assess if new version addresses their needs
- No easy way to see what's new in each release

## Evidence

1. **Missing changelog file:**
   ```bash
   ls CHANGELOG.md docs/CHANGELOG.md  # Returns: No such file
   ```

2. **Auto-generated release notes** (`build.yml`, line 88):
   ```yaml
   - name: Create Release
     uses: softprops/action-gh-release@v1
     with:
       files: artifacts/*
       generate_release_notes: true  # Auto-generated from commits
   ```

3. **Third-party has changelog** but main project does not:
   - `third_party/libtropic/CHANGELOG.md` exists (from libtropic dependency)
   - Main project `CHANGELOG.md` does not exist

## Recommended Fix

1. **Create CHANGELOG.md** following Keep a Changelog format:
   ```markdown
   # Changelog

   ## [Unreleased]
   ### Added
   - New feature X

   ### Changed
   - Changed Y

   ## [v0.5.0] - 2026-04-27
   ### Added
   - FIDO2/WebAuthn support
   - TOTP authenticator
   ```

2. **Add release template** (`.github/ISSUE_TEMPLATE/release.md` or `.github/release-template.md`):
   ```markdown
   ## Release vX.Y.Z

   ### Highlights
   - Major feature 1
   - Major feature 2

   ### Breaking Changes
   - List any breaking changes

   ### Fixed
   - Bug fixes

   ### Contributors
   - List contributors
   ```

3. **Update workflow** to use curated release notes:
   ```yaml
   - name: Create Release
     uses: softprops/action-gh-release@v1
     with:
       files: artifacts/*
       body_path: RELEASE_NOTES.md  # Use curated notes instead of auto-generated
   ```

4. **Add contribution guideline** for changelog updates:
   - Update CHANGELOG.md in each PR
   - Use standard categories (Added, Changed, Fixed, etc.)

**Estimated effort:** 1 hour to create initial CHANGELOG.md and template

## References

- [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
- [GitHub Release Notes](https://docs.github.com/en/repositories/releasing-projects-on-github/automatically-generated-release-notes)
- Existing example: `third_party/libtropic/CHANGELOG.md`

</content>