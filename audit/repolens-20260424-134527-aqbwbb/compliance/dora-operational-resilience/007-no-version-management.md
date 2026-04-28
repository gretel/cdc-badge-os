---
title: "[LOW] No Version Management and Release Notes Documentation"
severity: LOW
domain: Release Management
lens: dora-release-management
labels:
  - "audit:compliance/dora-operational-resilience"
---

## Summary
The repository lacks:
- Clear version management strategy in code
- Comprehensive release notes with security-relevant changes
- Version tracking for critical dependencies
- Change management documentation

Key searches performed:
- `grep -rn 'VERSION\|firmware.*version' --include='*.cpp' --include='*.h'` - no application-level version found
- `find . -name 'CHANGELOG*' -maxdepth 2` - no CHANGELOG in root directory

## Impact
For financial entities:
- Cannot track security-relevant changes across versions
- No clear version identification for compliance audits
- Difficult to assess impact of dependency updates
- Unclear upgrade path for production deployments

## Evidence
Current state:
- `.github/workflows/build.yml` line 38-48: Version extracted from Git tags for artifact naming
- `README.md` line 7: "Early Alpha" - no clear version status
- No `CHANGELOG.md` in root directory
- No version defined in main code (only build artifacts have versions)

From `.github/workflows/build.yml`:
```yaml
- name: Get version info
  id: version
  run: |
    if [[ "$GIT_REF" == refs/tags/* ]]; then
      VERSION=${GIT_REF#refs/tags/}
    else
      VERSION=$(git rev-parse --short HEAD)
    fi
```
Version is Git-tag based, but no in-code version for runtime identification.

## Recommended Fix

1. **Create `VERSION.md`** with:
   - Current version number
   - Versioning scheme (Semantic Versioning)
   - Version status (Alpha, Beta, Stable)

2. **Create `CHANGELOG.md`** with:
   - Release history
   - Security-relevant changes highlighted
   - Breaking changes clearly marked

3. **Add runtime version to firmware**:
   - Define `APP_VERSION` in `components/cdc_core/`
   - Add `VERSION` serial command to query firmware version

4. **Create `docs/RELEASE_PROCESS.md`** with:
   - Release checklist
   - Security review requirements
   - Version bump procedure

## References
- DORA Regulation (EU) 2022/2554, Article 6 - ICT risk management (change management)
- Semantic Versioning 2.0.0 (semver.org)
- Keep a Changelog (keepachangelog.com)
