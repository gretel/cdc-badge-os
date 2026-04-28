---
title: "[MEDIUM] Mutable GitHub Actions Runner Reference (ubuntu-latest)"
severity: MEDIUM
domain: deployment-safety
lens: deployment-safety
labels:
  - "audit:devops/deployment-safety"
---

## Summary

GitHub Actions workflows use `ubuntu-latest` as the runner reference, which is a mutable tag that can change to a new Ubuntu version at any time. This can lead to:

1. **Non-deterministic builds**: New Ubuntu version may have different compilers, libraries, or behaviors
2. **Breaking changes**: New Ubuntu version may not be compatible with existing build tools
3. **Debugging difficulty**: Build failures may appear due to infrastructure changes, not code changes

**Evidence:**
- `build.yml` uses: `runs-on: ubuntu-latest` (line 16, 73)
- `deploy-pages.yml` uses: `runs-on: ubuntu-24.04` (line 31) - *better, but still version-based*

## Impact

**Reliability:**
- Builds may break when GitHub updates `ubuntu-latest` to a new Ubuntu version
- ESP-IDF and PlatformIO tools may have different behavior on new Ubuntu versions
- Harder to reproduce build failures locally

**Maintenance:**
- Unexpected build failures require investigation
- May need to update build scripts to work with new Ubuntu version

**Security:**
- New Ubuntu version may introduce different security behaviors

## Evidence

**build.yml** (lines 16, 73):
```yaml
jobs:
  build:
    runs-on: ubuntu-latest  # Mutable reference

  release:
    needs: build
    runs-on: ubuntu-latest  # Mutable reference
```

**deploy-pages.yml** (line 31):
```yaml
jobs:
  deploy:
    runs-on: ubuntu-24.04  # Version reference (better, but still has risks)
```

## Recommended Fix

Pin to a specific Ubuntu version that is known to work:

1. **Update build.yml:**
   ```yaml
   jobs:
     build:
       runs-on: ubuntu-22.04  # or ubuntu-24.04
   
     release:
       runs-on: ubuntu-22.04  # Match the build runner
   ```

2. **Update deploy-pages.yml** (already using version):
   ```yaml
   jobs:
     deploy:
       runs-on: ubuntu-24.04  # Already good, but consider pinning to specific version
   ```

3. **Add version documentation:**
   - Document why specific Ubuntu version was chosen
   - Plan for version upgrades (e.g., every 6 months)

4. **Test on multiple runners:**
   - Consider running CI on both `ubuntu-22.04` and `ubuntu-24.04` to catch compatibility issues

**Estimated effort:** 15 minutes to update, 30 minutes to test

## References

- [GitHub Actions Runner Versions](https://docs.github.com/en/actions/using-workflows/workflow-syntax-for-github-actions#choosing-github-hosted-runners)
- [GitHub Actions Runner Images](https://github.com/actions/runner-images)
- ESP-IDF build requirements documentation

</content>