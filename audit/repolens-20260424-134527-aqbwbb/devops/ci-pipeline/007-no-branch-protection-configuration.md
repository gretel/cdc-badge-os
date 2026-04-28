---
title: "[HIGH] Missing branch protection configuration"
severity: HIGH
domain: devops
lens: ci-pipeline
labels:
  - "audit:devops/ci-pipeline"
---

## Summary
The repository has no branch protection rules configured. The main branch can accept direct pushes without requiring CI checks, code review, or a minimum number of reviewers. This is especially critical for a security-focused project.

**Evidence:**
- No `branch-protection-rules.yml` or similar file in `.github/`
- No `.github/CODEOWNERS` file for automatic reviewers
- CI workflow allows pushes to `main` without gating

## Impact
- Direct pushes to main can bypass CI checks
- Code can be merged without review
- No minimum reviewer requirement
- Force pushes can overwrite history
- Security-critical changes may not get proper review

## Evidence
```yaml
# build.yml triggers on:
on:
  push:
    branches: [main, master, release, develop, feature/*]
  pull_request:
    branches: [main, master, release]
# No branch protection to enforce PRs for main
```

## Recommended Fix
Create a branch protection configuration file or use GitHub's web UI:

**Option 1: Using GitHub web UI (recommended for small repos):**
1. Go to Settings > Branches > Add rule
2. Pattern: `main`
3. Enable:
   - Require pull request reviews before merging (1-2 reviewers)
   - Require status checks to pass (build)
   - Include administrators
   - Require linear history (optional)
   - Allow force pushes: disabled

**Option 2: Using terraform/github-actions:**
Create `.github/branch-protection.yml` using a tool like `branch-protection-action`.

Also create `.github/CODEOWNERS`:
```
# Code owners for CDC Badge OS
* @your-org/core-team
/components/mod_fido2/ @your-org/fido-expert
/components/mod_gpg/ @your-org/gpg-expert
```

## References
- [GitHub Branch Protection](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/defining-the-mergeability-of-pull-requests/managing-a-branch-protection-rule)
- [CODEOWNERS](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-code-owners)
