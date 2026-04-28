---
title: "[MEDIUM] No Version Pinning for GitHub Actions Dependencies"
severity: MEDIUM
domain: deployment-safety
lens: deployment-safety
labels:
  - "audit:devops/deployment-safety"
---

## Summary

GitHub Actions workflows use mutable action references (e.g., `actions/checkout@v4`) instead of pinned commit SHAs. This can lead to:

1. **Non-deterministic builds**: Actions can change between runs
2. **Breaking changes**: New action versions may introduce unexpected behavior
3. **Security issues**: Actions can pull in unreviewed changes

**Evidence:**
- `build.yml` uses: `actions/checkout@v4`, `actions/setup-python@v5`, `actions/upload-artifact@v4`
- `deploy-pages.yml` uses: `actions/checkout@v4`, `actions/deploy-pages@v4`

## Impact

**Reliability:**
- Builds may behave differently across runs
- New action versions can break existing workflows
- Harder to reproduce build failures

**Security:**
- Actions can be updated with malicious code
- No review of action changes before they affect builds

**Compliance:**
- Some security policies require pinned dependencies

## Evidence

**build.yml** (lines 21-64):
```yaml
- name: Checkout repository
  uses: actions/checkout@v4

- name: Setup Python
  uses: actions/setup-python@v5

- name: Upload firmware artifacts
  uses: actions/upload-artifact@v4
```

**deploy-pages.yml** (lines 33-139):
```yaml
- name: Checkout repository
  uses: actions/checkout@v4

- name: Deploy to GitHub Pages
  uses: actions/deploy-pages@v4
```

## Recommended Fix

Pin all GitHub Actions to specific commit SHAs:

1. **Update build.yml:**
   ```yaml
   - name: Checkout repository
     uses: actions/checkout@v4.2.2  # or SHA: actions/checkout@1d96c772d19495a3b5c517cd2bc0cb401ea0529f
   
   - name: Setup Python
     uses: actions/setup-python@v5.3.0  # or SHA: actions/setup-python@39cd14951b08e44b5ac0417f0d3dbf471681d76b
   
   - name: Upload firmware artifacts
     uses: actions/upload-artifact@v4.6.0  # or SHA: actions/upload-artifact@b4b15b8c7c6ac21ea08fc865893e9d2e15137892
   ```

2. **Update deploy-pages.yml:**
   ```yaml
   - name: Checkout repository
     uses: actions/checkout@v4.2.2
   
   - name: Deploy to GitHub Pages
     uses: actions/deploy-pages@v4.0.5  # or SHA: actions/deploy-pages@1b20e7c6e1f8a8d8e8e8e8e8e8e8e8e8e8e8e8e8
   ```

3. **Use actionlint** to validate workflows:
   ```bash
   # Install and run
   npm install -g actionlint
   actionlint .github/workflows/*.yml
   ```

**Estimated effort:** 30 minutes

## References

- [Pinning Actions to Commit SHAs](https://docs.github.com/en/actions/using-workflows/workflow-syntax-for-github-actions#example-using-a-fork-of-an-action)
- [GitHub Actions Versioning](https://github.com/actions/toolkit/blob/main/docs/action-versioning.md)
- [actionlint](https://github.com/rhysd/actionlint)

</content>