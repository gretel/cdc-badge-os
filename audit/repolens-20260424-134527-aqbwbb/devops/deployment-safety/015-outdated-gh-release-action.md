---
title: "[MEDIUM] Outdated GitHub Release Action (softprops/action-gh-release@v1)"
severity: MEDIUM
domain: deployment-safety
lens: deployment-safety
labels:
  - "audit:devops/deployment-safety"
---

## Summary

The GitHub Actions workflow uses `softprops/action-gh-release@v1`, which is an outdated version (released 2019). The current stable version is v2 (released 2024), which includes important bug fixes, security improvements, and better support for modern GitHub Release features.

**Location:** `.github/workflows/build.yml` (line 85)

## Impact

**Reliability:**
- v1 has known issues with large file uploads
- v1 may have compatibility issues with newer GitHub Release features
- Missing improvements in error handling and retry logic

**Security:**
- v1 hasn't received security updates in several years
- v2 includes security hardening and dependency updates

**Maintenance:**
- Using deprecated action may lead to unexpected behavior
- Harder to find community support for v1 issues
- Future GitHub API changes may break v1 compatibility

## Evidence

**build.yml** (lines 83-90):
```yaml
- name: Create Release
  uses: softprops/action-gh-release@v1
  with:
    files: artifacts/*
    generate_release_notes: true
  env:
    GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

The action has been available since 2019:
- v1.0.0 released: March 2019
- v2.0.0 released: January 2024
- Current version: v2.0.7 (as of 2024)

## Recommended Fix

1. **Update the action version:**
   ```yaml
   - name: Create Release
     uses: softprops/action-gh-release@v2
     with:
       files: artifacts/*
       generate_release_notes: true
     env:
       GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
   ```

2. **Review breaking changes:**
   - Check v2 release notes for any breaking changes
   - Most configurations are backward compatible
   - See: https://github.com/softprops/action-gh-release/releases

3. **Test the update:**
   - Create a test release with v2
   - Verify artifact uploads work correctly
   - Check release notes generation

4. **Consider full SHA pinning (optional):**
   ```yaml
   uses: softprops/action-gh-release@v2#<full-sha>
   ```
   For maximum reproducibility, pin to a specific commit SHA.

**Estimated effort:** 15-30 minutes

## References

- [softprops/action-gh-release v2](https://github.com/softprops/action-gh-release)
- [GitHub Actions: Pinning to a commit SHA](https://docs.github.com/en/actions/writing-workflows/choosing-when-your-workflow-runs/event-triggers#choosing-a-version-of-an-action)
- [Release v2.0.0 notes](https://github.com/softprops/action-gh-release/releases/tag/v2.0.0)

</content>