---
title: "[LOW] GitHub Actions workflows use outdated action versions"
severity: LOW
domain: ci-cd
lens: upgrade-paths
labels:
  - "audit:maintainability/upgrade-paths"
---

## Summary
Several GitHub Actions used in the CI/CD workflows are on older major versions. While not critical, updating them ensures access to latest features, bug fixes, and performance improvements.

**Evidence:**
- File: `.github/workflows/build.yml` and `.github/workflows/deploy-pages.yml`
- Current versions vs latest available:
  - `actions/checkout@v4` → latest is `v6` (v5 also available)
  - `actions/setup-python@v5` → latest is `v6`
  - `actions/upload-artifact@v4` → latest is `v7`
  - `actions/download-artifact@v4` → latest is `v8`
  - `actions/configure-pages@v4` → latest is `v6`
  - `actions/upload-pages-artifact@v3` → latest is `v5`
  - `actions/deploy-pages@v4` → latest is `v5`
  - `softprops/action-gh-release@v1` → latest is `v3`

## Impact
**Benefits of updating:**
- **actions/checkout v4→v6**: Improved submodules handling, better performance
- **actions/setup-python v5→v6**: Updated caching, Python 3.13+ support
- **artifact actions v4→v7/v8**: Faster uploads, better compression, retention improvements
- **softprops/action-gh-release v1→v3**: Better release notes, improved asset handling

**Risk:**
- Minimal - these are well-maintained actions with backward compatibility
- `softprops/action-gh-release` v2/v3 have minor API changes (check changelog)

## Evidence
From `.github/workflows/build.yml`:
```yaml
uses: actions/checkout@v4
uses: actions/setup-python@v5
uses: actions/upload-artifact@v4
uses: actions/download-artifact@v4
uses: softprops/action-gh-release@v1
```

From `.github/workflows/deploy-pages.yml`:
```yaml
uses: actions/checkout@v4
uses: actions/configure-pages@v4
uses: actions/upload-pages-artifact@v3
uses: actions/deploy-pages@v4
```

Latest versions (as of 2026-04-27):
- `actions/checkout`: v6.0.2
- `actions/setup-python`: v6.2.0
- `actions/upload-artifact`: v7.0.1
- `actions/download-artifact`: v8.0.1
- `actions/configure-pages`: v6.0.0
- `actions/upload-pages-artifact`: v5.0.0
- `actions/deploy-pages`: v5.0.0
- `softprops/action-gh-release`: v3.0.0

## Recommended Fix
**Update `build.yml`:**
```yaml
uses: actions/checkout@v6
uses: actions/setup-python@v6
uses: actions/upload-artifact@v7
uses: actions/download-artifact@v8
uses: softprops/action-gh-release@v3
```

**Update `deploy-pages.yml`:**
```yaml
uses: actions/checkout@v6
uses: actions/configure-pages@v6
uses: actions/upload-pages-artifact@v5
uses: actions/deploy-pages@v5
```

**Test:**
1. Create a test branch and push to trigger workflows
2. Verify build and deploy jobs pass
3. Check release creation works with v3 of softprops action

**Note:** `softprops/action-gh-release` v2+ has breaking changes - review [migration guide](https://github.com/softprops/action-gh-release#migration-from-v1) before updating.

## References
- [actions/checkout releases](https://github.com/actions/checkout/tags)
- [actions/setup-python releases](https://github.com/actions/setup-python/tags)
- [actions/upload-artifact releases](https://github.com/actions/upload-artifact/tags)
- [softprops/action-gh-release releases](https://github.com/softprops/action-gh-release/tags)
